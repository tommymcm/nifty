# Parser Directory Plan

## Purpose
Responsible for reading and parsing Doxygen XML files into typed TypeScript objects. This layer transforms raw XML into a clean, strongly-typed data model that the search engine can work with efficiently.

## Structure
```
parser/
├── index.ts                  # Public API exports
├── types.ts                  # TypeScript interfaces for entities
├── doxygen-parser.ts         # Main parser orchestrator
├── xml-reader.ts             # XML file discovery and loading
├── entity-builders/
│   ├── base.ts              # Base entity builder
│   ├── class-builder.ts     # Build ClassEntity from XML
│   ├── function-builder.ts  # Build FunctionEntity from XML
│   ├── namespace-builder.ts # Build NamespaceEntity from XML
│   ├── enum-builder.ts      # Build EnumEntity from XML
│   └── factory.ts           # Builder factory
├── extractors/
│   ├── description.ts       # Extract descriptions from XML
│   ├── location.ts          # Extract file/line info
│   ├── parameters.ts        # Extract function parameters
│   ├── inheritance.ts       # Extract base classes
│   └── members.ts           # Extract class members
├── cache/
│   ├── cache-manager.ts     # Cache orchestration (Phase 2)
│   ├── cache-storage.ts     # Cache persistence (Phase 2)
│   └── cache-validator.ts   # Cache validation (Phase 2)
└── utils/
    ├── xml-utils.ts         # XML parsing utilities
    └── path-utils.ts        # Path normalization utilities
```

## Core Type Definitions (`types.ts`)

```typescript
/**
 * Base entity that all Doxygen elements extend
 */
export interface DoxygenEntity {
  id: string;                      // Unique identifier from Doxygen
  name: string;                    // Simple name (e.g., "MyClass")
  qualifiedName: string;           // Fully qualified name (e.g., "ns::MyClass")
  kind: EntityKind;                // Type of entity
  file: string;                    // Source file path
  line: number;                    // Line number in source
  briefDescription?: string;       // Brief description
  detailedDescription?: string;    // Detailed description
  visibility?: Visibility;         // public/protected/private
}

/**
 * Supported entity types
 */
export type EntityKind = 
  | 'class' 
  | 'struct' 
  | 'function' 
  | 'method'
  | 'namespace' 
  | 'enum' 
  | 'typedef'
  | 'variable'
  | 'define';

/**
 * Visibility levels
 */
export type Visibility = 'public' | 'protected' | 'private';

/**
 * Class or struct entity
 */
export interface ClassEntity extends DoxygenEntity {
  kind: 'class' | 'struct';
  baseClasses: BaseClass[];        // Inheritance
  members: MemberReference[];      // Methods, fields, etc.
  templateParams?: TemplateParam[]; // Template parameters
  isAbstract: boolean;             // Has pure virtual methods
  namespace?: string;              // Parent namespace
}

/**
 * Base class information
 */
export interface BaseClass {
  name: string;                    // Base class name
  qualifiedName: string;           // Fully qualified name
  visibility: Visibility;          // Inheritance visibility
  isVirtual: boolean;              // Virtual inheritance
}

/**
 * Reference to a class member
 */
export interface MemberReference {
  id: string;                      // Member ID
  name: string;                    // Member name
  kind: EntityKind;                // Member type
  visibility: Visibility;          // Access level
}

/**
 * Template parameter
 */
export interface TemplateParam {
  name: string;                    // Parameter name
  type: string;                    // Parameter type (class, typename, etc.)
  defaultValue?: string;           // Default value if any
}

/**
 * Function or method entity
 */
export interface FunctionEntity extends DoxygenEntity {
  kind: 'function' | 'method';
  returnType: string;              // Return type
  parameters: Parameter[];         // Function parameters
  isConst: boolean;                // Const method
  isStatic: boolean;               // Static function/method
  isVirtual: boolean;              // Virtual method
  isInline: boolean;               // Inline function
  isPureVirtual: boolean;          // Pure virtual method
  parentClass?: string;            // Parent class (for methods)
  namespace?: string;              // Parent namespace
  exceptions?: string[];           // Thrown exceptions
}

/**
 * Function parameter
 */
export interface Parameter {
  name: string;                    // Parameter name
  type: string;                    // Parameter type
  defaultValue?: string;           // Default value
  description?: string;            // Parameter description
}

/**
 * Namespace entity
 */
export interface NamespaceEntity extends DoxygenEntity {
  kind: 'namespace';
  members: MemberReference[];      // Namespace members
  parentNamespace?: string;        // Parent namespace
}

/**
 * Enum entity
 */
export interface EnumEntity extends DoxygenEntity {
  kind: 'enum';
  values: EnumValue[];             // Enum values
  isStrong: boolean;               // enum class
  underlyingType?: string;         // Underlying type
}

/**
 * Enum value
 */
export interface EnumValue {
  name: string;                    // Value name
  value?: string;                  // Explicit value
  description?: string;            // Value description
}

/**
 * Result of parsing operation
 */
export interface ParseResult {
  entities: DoxygenEntity[];       // All parsed entities
  metadata: ParseMetadata;         // Parse metadata
  errors: ParseError[];            // Non-fatal errors
}

/**
 * Metadata about the parse operation
 */
export interface ParseMetadata {
  doxygenVersion: string;          // Doxygen version used
  generatedAt: Date;               // When XML was generated
  totalFiles: number;              // Number of XML files processed
  totalEntities: number;           // Total entities parsed
  parseTimeMs: number;             // Time taken to parse
  fromCache: boolean;              // Was cache used
}

/**
 * Parse error (non-fatal)
 */
export interface ParseError {
  file: string;                    // File where error occurred
  entityId?: string;               // Entity ID if applicable
  message: string;                 // Error message
  severity: 'warning' | 'error';   // Severity level
}

/**
 * Options for parsing
 */
export interface ParseOptions {
  entityTypes?: EntityKind[];      // Only parse these types
  includePrivate?: boolean;        // Include private members
  includeDescriptions?: boolean;   // Parse descriptions
  useCache?: boolean;              // Use cached results
  parallel?: boolean;              // Parse files in parallel
  maxFiles?: number;               // Limit files processed (for testing)
}
```

## Main Parser (`doxygen-parser.ts`)

```typescript
import { XMLParser } from 'fast-xml-parser';
import { findDoxygenFiles, readXMLFile } from './xml-reader';
import { EntityBuilderFactory } from './entity-builders/factory';
import { ParseResult, ParseOptions, DoxygenEntity, ParseError } from './types';

/**
 * Main parser class that orchestrates the parsing process
 */
export class DoxygenParser {
  private options: Required<ParseOptions>;
  private xmlParser: XMLParser;
  private errors: ParseError[] = [];
  
  constructor(options: ParseOptions = {}) {
    this.options = this.mergeOptions(options);
    this.xmlParser = this.createXMLParser();
  }
  
  /**
   * Parse Doxygen XML directory
   */
  async parse(xmlPath: string): Promise<ParseResult> {
    const startTime = performance.now();
    this.errors = [];
    
    try {
      // 1. Find all XML files
      const files = await findDoxygenFiles(xmlPath);
      console.log(`Found ${files.length} XML files`);
      
      // 2. Parse index.xml first to get structure
      const indexFile = files.find(f => f.endsWith('index.xml'));
      if (!indexFile) {
        throw new Error('index.xml not found in Doxygen output');
      }
      
      const index = await this.parseIndexFile(indexFile);
      
      // 3. Filter compounds by requested types
      const compoundsToLoad = this.filterCompounds(index.compounds);
      console.log(`Loading ${compoundsToLoad.length} compounds`);
      
      // 4. Parse compound files
      const entities = await this.parseCompounds(
        xmlPath,
        compoundsToLoad
      );
      
      // 5. Build result
      const parseTimeMs = performance.now() - startTime;
      
      return {
        entities,
        metadata: {
          doxygenVersion: index.version,
          generatedAt: new Date(),
          totalFiles: files.length,
          totalEntities: entities.length,
          parseTimeMs,
          fromCache: false
        },
        errors: this.errors
      };
      
    } catch (error) {
      throw new Error(`Failed to parse Doxygen XML: ${error.message}`);
    }
  }
  
  /**
   * Parse index.xml to get document structure
   */
  private async parseIndexFile(indexPath: string): Promise<DoxygenIndex> {
    const content = await readXMLFile(indexPath);
    const parsed = this.xmlParser.parse(content);
    
    const index = parsed.doxygenindex;
    if (!index) {
      throw new Error('Invalid index.xml structure');
    }
    
    // Extract compounds
    const compoundElements = Array.isArray(index.compound) 
      ? index.compound 
      : [index.compound];
    
    const compounds = compoundElements
      .filter(Boolean)
      .map(c => ({
        refid: c['@_refid'],
        kind: c['@_kind'],
        name: c.name
      }));
    
    return {
      version: index['@_version'] || 'unknown',
      compounds
    };
  }
  
  /**
   * Filter compounds by requested entity types
   */
  private filterCompounds(compounds: CompoundInfo[]): CompoundInfo[] {
    if (!this.options.entityTypes || this.options.entityTypes.length === 0) {
      return compounds;
    }
    
    return compounds.filter(c => 
      this.shouldLoadCompound(c.kind)
    );
  }
  
  /**
   * Check if compound kind should be loaded
   */
  private shouldLoadCompound(kind: string): boolean {
    const typeMap: Record<string, EntityKind[]> = {
      'class': ['class', 'method'],
      'struct': ['struct', 'method'],
      'namespace': ['namespace'],
      'file': ['function'],
      'enum': ['enum']
    };
    
    const entityTypes = typeMap[kind] || [];
    return entityTypes.some(t => 
      !this.options.entityTypes || this.options.entityTypes.includes(t)
    );
  }
  
  /**
   * Parse compound XML files
   */
  private async parseCompounds(
    xmlPath: string,
    compounds: CompoundInfo[]
  ): Promise<DoxygenEntity[]> {
    const entities: DoxygenEntity[] = [];
    
    // Sequential parsing for Phase 1
    // TODO: Parallel parsing in Phase 2
    for (const compound of compounds) {
      try {
        const compoundEntities = await this.parseCompoundFile(
          xmlPath,
          compound
        );
        entities.push(...compoundEntities);
      } catch (error) {
        this.errors.push({
          file: `${compound.refid}.xml`,
          message: `Failed to parse compound: ${error.message}`,
          severity: 'error'
        });
      }
    }
    
    return entities;
  }
  
  /**
   * Parse a single compound file
   */
  private async parseCompoundFile(
    xmlPath: string,
    compound: CompoundInfo
  ): Promise<DoxygenEntity[]> {
    const filePath = `${xmlPath}/${compound.refid}.xml`;
    const content = await readXMLFile(filePath);
    const parsed = this.xmlParser.parse(content);
    
    const compoundDef = parsed.doxygen?.compounddef;
    if (!compoundDef) {
      throw new Error('Invalid compound XML structure');
    }
    
    const entities: DoxygenEntity[] = [];
    
    // Build main compound entity (class, namespace, etc.)
    const builder = EntityBuilderFactory.getBuilder(compoundDef['@_kind']);
    const mainEntity = builder.build(compoundDef);
    
    if (this.shouldIncludeEntity(mainEntity)) {
      entities.push(mainEntity);
    }
    
    // Extract member entities (methods, functions, etc.)
    const members = this.extractMembers(compoundDef, mainEntity);
    entities.push(...members.filter(m => this.shouldIncludeEntity(m)));
    
    return entities;
  }
  
  /**
   * Extract member entities from compound
   */
  private extractMembers(
    compoundDef: any,
    parent: DoxygenEntity
  ): DoxygenEntity[] {
    const members: DoxygenEntity[] = [];
    
    // Get all section definitions (public, protected, private)
    const sections = Array.isArray(compoundDef.sectiondef)
      ? compoundDef.sectiondef
      : compoundDef.sectiondef ? [compoundDef.sectiondef] : [];
    
    for (const section of sections) {
      const memberDefs = Array.isArray(section.memberdef)
        ? section.memberdef
        : section.memberdef ? [section.memberdef] : [];
      
      for (const memberDef of memberDefs) {
        try {
          const kind = memberDef['@_kind'];
          const builder = EntityBuilderFactory.getBuilder(kind);
          const member = builder.build(memberDef, parent);
          members.push(member);
        } catch (error) {
          this.errors.push({
            file: `${parent.id}.xml`,
            entityId: memberDef['@_id'],
            message: `Failed to parse member: ${error.message}`,
            severity: 'warning'
          });
        }
      }
    }
    
    return members;
  }
  
  /**
   * Check if entity should be included based on options
   */
  private shouldIncludeEntity(entity: DoxygenEntity): boolean {
    // Filter by type
    if (this.options.entityTypes && 
        !this.options.entityTypes.includes(entity.kind)) {
      return false;
    }
    
    // Filter by visibility
    if (!this.options.includePrivate && 
        entity.visibility === 'private') {
      return false;
    }
    
    return true;
  }
  
  /**
   * Create XML parser with appropriate options
   */
  private createXMLParser(): XMLParser {
    return new XMLParser({
      ignoreAttributes: false,
      attributeNamePrefix: '@_',
      parseAttributeValue: false,
      trimValues: true,
      cdataPropName: '__cdata',
      textNodeName: '#text',
      parseTagValue: false,
      isArray: (name, jpath, isLeafNode, isAttribute) => {
        // These elements can appear multiple times
        return ['compound', 'member', 'memberdef', 'sectiondef', 
                'basecompoundref', 'param'].includes(name);
      }
    });
  }
  
  /**
   * Merge user options with defaults
   */
  private mergeOptions(options: ParseOptions): Required<ParseOptions> {
    return {
      entityTypes: options.entityTypes || undefined,
      includePrivate: options.includePrivate ?? false,
      includeDescriptions: options.includeDescriptions ?? true,
      useCache: options.useCache ?? true,
      parallel: options.parallel ?? false,
      maxFiles: options.maxFiles || undefined
    };
  }
}

/**
 * Doxygen index structure
 */
interface DoxygenIndex {
  version: string;
  compounds: CompoundInfo[];
}

/**
 * Compound reference from index
 */
interface CompoundInfo {
  refid: string;      // Reference ID (filename without .xml)
  kind: string;       // Compound kind (class, struct, namespace, etc.)
  name: string;       // Compound name
}

/**
 * Convenience function for simple parsing
 */
export async function parseDoxygenXML(
  xmlPath: string,
  options?: ParseOptions
): Promise<ParseResult> {
  const parser = new DoxygenParser(options);
  return parser.parse(xmlPath);
}
```

## XML File Reader (`xml-reader.ts`)

```typescript
import { readdir, readFile, stat } from 'fs/promises';
import { join } from 'path';

/**
 * Find all Doxygen XML files in directory
 */
export async function findDoxygenFiles(xmlPath: string): Promise<string[]> {
  try {
    const entries = await readdir(xmlPath);
    
    const xmlFiles = entries
      .filter(entry => entry.endsWith('.xml'))
      .map(entry => join(xmlPath, entry));
    
    return xmlFiles;
  } catch (error) {
    throw new Error(`Cannot read XML directory: ${error.message}`);
  }
}

/**
 * Read XML file content
 */
export async function readXMLFile(filePath: string): Promise<string> {
  try {
    return await readFile(filePath, 'utf-8');
  } catch (error) {
    throw new Error(`Cannot read XML file ${filePath}: ${error.message}`);
  }
}

/**
 * Validate that path is a Doxygen XML directory
 */
export async function validateDoxygenPath(xmlPath: string): Promise<void> {
  // Check directory exists
  try {
    const stats = await stat(xmlPath);
    if (!stats.isDirectory()) {
      throw new Error('Path is not a directory');
    }
  } catch (error) {
    throw new Error(`Invalid XML path: ${error.message}`);
  }
  
  // Check for index.xml
  try {
    await stat(join(xmlPath, 'index.xml'));
  } catch (error) {
    throw new Error('index.xml not found - not a valid Doxygen XML directory');
  }
}
```

## Entity Builders

### Base Builder (`entity-builders/base.ts`)

```typescript
import { DoxygenEntity } from '../types';

/**
 * Base class for entity builders
 */
export abstract class EntityBuilder<T extends DoxygenEntity = DoxygenEntity> {
  /**
   * Build entity from XML node
   */
  abstract build(xmlNode: any, parent?: DoxygenEntity): T;
  
  /**
   * Extract common fields shared by all entities
   */
  protected buildBase(xmlNode: any): Partial<DoxygenEntity> {
    return {
      id: xmlNode['@_id'] || '',
      name: this.extractName(xmlNode),
      qualifiedName: this.extractQualifiedName(xmlNode),
      file: this.extractFile(xmlNode),
      line: this.extractLine(xmlNode),
      briefDescription: this.extractBriefDescription(xmlNode),
      detailedDescription: this.extractDetailedDescription(xmlNode),
      visibility: this.extractVisibility(xmlNode)
    };
  }
  
  protected extractName(xmlNode: any): string {
    return xmlNode.compoundname || xmlNode.name || '';
  }
  
  protected extractQualifiedName(xmlNode: any): string {
    return xmlNode.qualifiedname || xmlNode.compoundname || xmlNode.name || '';
  }
  
  protected extractFile(xmlNode: any): string {
    const location = xmlNode.location;
    return location?.['@_file'] || '';
  }
  
  protected extractLine(xmlNode: any): number {
    const location = xmlNode.location;
    const line = location?.['@_line'];
    return line ? parseInt(line, 10) : 0;
  }
  
  protected extractBriefDescription(xmlNode: any): string | undefined {
    const brief = xmlNode.briefdescription;
    if (!brief) return undefined;
    
    return this.extractText(brief);
  }
  
  protected extractDetailedDescription(xmlNode: any): string | undefined {
    const detailed = xmlNode.detaileddescription;
    if (!detailed) return undefined;
    
    return this.extractText(detailed);
  }
  
  protected extractVisibility(xmlNode: any): 'public' | 'protected' | 'private' | undefined {
    const prot = xmlNode['@_prot'];
    if (prot === 'public' || prot === 'protected' || prot === 'private') {
      return prot;
    }
    return undefined;
  }
  
  /**
   * Extract text content from XML node
   */
  protected extractText(node: any): string {
    if (typeof node === 'string') return node;
    if (!node) return '';
    
    // Handle para elements
    if (node.para) {
      const paras = Array.isArray(node.para) ? node.para : [node.para];
      return paras
        .map(p => this.extractText(p))
        .join('\n')
        .trim();
    }
    
    // Handle text nodes
    if (node['#text']) {
      return node['#text'];
    }
    
    // Handle mixed content
    if (typeof node === 'object') {
      return Object.values(node)
        .map(v => this.extractText(v))
        .join(' ')
        .trim();
    }
    
    return '';
  }
}
```

### Class Builder (`entity-builders/class-builder.ts`)

```typescript
import { EntityBuilder } from './base';
import { ClassEntity, BaseClass, MemberReference } from '../types';
import { extractBaseClasses, extractMembers } from '../extractors';

export class ClassEntityBuilder extends EntityBuilder<ClassEntity> {
  build(xmlNode: any): ClassEntity {
    const base = this.buildBase(xmlNode);
    const kind = xmlNode['@_kind'] as 'class' | 'struct';
    
    return {
      ...base,
      kind,
      baseClasses: extractBaseClasses(xmlNode),
      members: extractMembers(xmlNode),
      templateParams: this.extractTemplateParams(xmlNode),
      isAbstract: this.checkIsAbstract(xmlNode),
      namespace: this.extractNamespace(base.qualifiedName)
    } as ClassEntity;
  }
  
  private extractTemplateParams(xmlNode: any): any[] | undefined {
    const templateParamList = xmlNode.templateparamlist;
    if (!templateParamList?.param) return undefined;
    
    const params = Array.isArray(templateParamList.param)
      ? templateParamList.param
      : [templateParamList.param];
    
    return params.map(p => ({
      name: p.declname || '',
      type: p.type || '',
      defaultValue: p.defval
    }));
  }
  
  private checkIsAbstract(xmlNode: any): boolean {
    // Check if any member is pure virtual
    const sections = Array.isArray(xmlNode.sectiondef)
      ? xmlNode.sectiondef
      : xmlNode.sectiondef ? [xmlNode.sectiondef] : [];
    
    for (const section of sections) {
      const members = Array.isArray(section.memberdef)
        ? section.memberdef
        : section.memberdef ? [section.memberdef] : [];
      
      for (const member of members) {
        if (member['@_virt'] === 'pure-virtual') {
          return true;
        }
      }
    }
    
    return false;
  }
  
  private extractNamespace(qualifiedName: string): string | undefined {
    const lastColon = qualifiedName.lastIndexOf('::');
    if (lastColon === -1) return undefined;
    return qualifiedName.substring(0, lastColon);
  }
}
```

### Function Builder (`entity-builders/function-builder.ts`)

```typescript
import { EntityBuilder } from './base';
import { FunctionEntity, Parameter, DoxygenEntity } from '../types';
import { extractParameters } from '../extractors';

export class FunctionEntityBuilder extends EntityBuilder<FunctionEntity> {
  build(xmlNode: any, parent?: DoxygenEntity): FunctionEntity {
    const base = this.buildBase(xmlNode);
    
    // Determine if this is a method or standalone function
    const kind = parent?.kind === 'class' || parent?.kind === 'struct'
      ? 'method'
      : 'function';
    
    return {
      ...base,
      kind,
      returnType: this.extractReturnType(xmlNode),
      parameters: extractParameters(xmlNode),
      isConst: xmlNode['@_const'] === 'yes',
      isStatic: xmlNode['@_static'] === 'yes',
      isVirtual: xmlNode['@_virt'] === 'virtual' || xmlNode['@_virt'] === 'pure-virtual',
      isInline: xmlNode['@_inline'] === 'yes',
      isPureVirtual: xmlNode['@_virt'] === 'pure-virtual',
      parentClass: kind === 'method' ? parent?.qualifiedName : undefined,
      namespace: this.extractNamespace(base.qualifiedName),
      exceptions: this.extractExceptions(xmlNode)
    } as FunctionEntity;
  }
  
  private extractReturnType(xmlNode: any): string {
    const type = xmlNode.type;
    if (!type) return 'void';
    
    return this.extractText(type);
  }
  
  private extractNamespace(qualifiedName: string): string | undefined {
    const lastColon = qualifiedName.lastIndexOf('::');
    if (lastColon === -1) return undefined;
    return qualifiedName.substring(0, lastColon);
  }
  
  private extractExceptions(xmlNode: any): string[] | undefined {
    // Doxygen can document exceptions in the description
    // This is a simplified extraction
    const detailed = xmlNode.detaileddescription;
    if (!detailed) return undefined;
    
    // Look for @throws or @exception tags
    // This is a placeholder - actual implementation would parse XML structure
    return undefined;
  }
}
```

### Builder Factory (`entity-builders/factory.ts`)

```typescript
import { EntityBuilder } from './base';
import { ClassEntityBuilder } from './class-builder';
import { FunctionEntityBuilder } from './function-builder';
import { NamespaceEntityBuilder } from './namespace-builder';
import { EnumEntityBuilder } from './enum-builder';

export class EntityBuilderFactory {
  /**
   * Get appropriate builder for entity kind
   */
  static getBuilder(kind: string): EntityBuilder {
    switch (kind) {
      case 'class':
      case 'struct':
        return new ClassEntityBuilder();
      
      case 'function':
        return new FunctionEntityBuilder();
      
      case 'namespace':
        return new NamespaceEntityBuilder();
      
      case 'enum':
        return new EnumEntityBuilder();
      
      default:
        throw new Error(`Unsupported entity kind: ${kind}`);
    }
  }
}
```

## Extractors

### Inheritance Extractor (`extractors/inheritance.ts`)

```typescript
import { BaseClass } from '../types';

/**
 * Extract base classes from class XML node
 */
export function extractBaseClasses(xmlNode: any): BaseClass[] {
  const baseCompoundRefs = xmlNode.basecompoundref;
  if (!baseCompoundRefs) return [];
  
  const refs = Array.isArray(baseCompoundRefs)
    ? baseCompoundRefs
    : [baseCompoundRefs];
  
  return refs
    .filter(Boolean)
    .map(ref => ({
      name: extractText(ref),
      qualifiedName: ref['@_refid'] || extractText(ref),
      visibility: (ref['@_prot'] || 'public') as any,
      isVirtual: ref['@_virt'] === 'virtual'
    }));
}

function extractText(node: any): string {
  if (typeof node === 'string') return node;
  if (node['#text']) return node['#text'];
  return '';
}
```

### Members Extractor (`extractors/members.ts`)

```typescript
import { MemberReference } from '../types';

/**
 * Extract member references from class XML node
 */
export function extractMembers(xmlNode: any): MemberReference[] {
  const members: MemberReference[] = [];
  
  const sections = Array.isArray(xmlNode.sectiondef)
    ? xmlNode.sectiondef
    : xmlNode.sectiondef ? [xmlNode.sectiondef] : [];
  
  for (const section of sections) {
    const memberDefs = Array.isArray(section.memberdef)
      ? section.memberdef
      : section.memberdef ? [section.memberdef] : [];
    
    for (const memberDef of memberDefs) {
      members.push({
        id: memberDef['@_id'] || '',
        name: memberDef.name || '',
        kind: memberDef['@_kind'] as any,
        visibility: (memberDef['@_prot'] || 'public') as any
      });
    }
  }
  
  return members;
}
```

### Parameters Extractor (`extractors/parameters.ts`)

```typescript
import { Parameter } from '../types';

/**
 * Extract function parameters from XML node
 */
export function extractParameters(xmlNode: any): Parameter[] {
  const params = xmlNode.param;
  if (!params) return [];
  
  const paramList = Array.isArray(params) ? params : [params];
  
  return paramList
    .filter(Boolean)
    .map(param => ({
      name: param.declname || param.defname || '',
      type: extractType(param.type),
      defaultValue: param.defval ? extractText(param.defval) : undefined,
      description: undefined // TODO: Extract from param documentation
    }));
}

function extractType(typeNode: any): string {
  if (!typeNode) return '';
  return extractText(typeNode);
}

function extractText(node: any): string {
  if (typeof node === 'string') return node;
  if (node['#text']) return node['#text'];
  if (typeof node === 'object') {
    return Object.values(node)
      .map(v => extractText(v))
      .join('')
      .trim();
  }
  return '';
}
```

## Testing Strategy

### Unit Tests

```typescript
describe('DoxygenParser', () => {
  test('parses index.xml', async () => {
    const parser = new DoxygenParser();
    const result = await parser.parse('./test-fixtures/simple');
    
    expect(result.entities.length).toBeGreaterThan(0);
    expect(result.errors.length).toBe(0);
  });
  
  test('filters by entity type', async () => {
    const parser = new DoxygenParser({
      entityTypes: ['class']
    });
    const result = await parser.parse('./test-fixtures/simple');
    
    expect(result.entities.every(e => e.kind === 'class')).toBe(true);
  });
  
  test('excludes private members by default', async () => {
    const parser = new DoxygenParser({
      includePrivate: false
    });
    const result = await parser.parse('./test-fixtures/visibility');
    
    expect(result.entities.every(e => e.visibility !== 'private')).toBe(true);
  });
});

describe('ClassEntityBuilder', () => {
  test('builds class entity from XML', () => {
    const builder = new ClassEntityBuilder();
    const xmlNode = loadFixture('class-simple.xml');
    
    const entity = builder.build(xmlNode);
    
    expect(entity.kind).toBe('class');
    expect(entity.name).toBeTruthy();
    expect(entity.file).toBeTruthy();
  });
  
  test('extracts base classes', () => {
    const builder = new ClassEntityBuilder();
    const xmlNode = loadFixture('class-inheritance.xml');
    
    const entity = builder.build(xmlNode);
    
    expect(entity.baseClasses.length).toBeGreaterThan(0);
    expect(entity.baseClasses[0].name).toBeTruthy();
  });
  
  test('identifies abstract classes', () => {
    const builder = new ClassEntityBuilder();
    const xmlNode = loadFixture('class-abstract.xml');
    
    const entity = builder.build(xmlNode);
    
    expect(entity.isAbstract).toBe(true);
  });
});

describe('FunctionEntityBuilder', () => {
  test('builds function entity from XML', () => {
    const builder = new FunctionEntityBuilder();
    const xmlNode = loadFixture('function-simple.xml');
    
    const entity = builder.build(xmlNode);
    
    expect(entity.kind).toBe('function');
    expect(entity.returnType).toBeTruthy();
  });
  
  test('extracts parameters', () => {
    const builder = new FunctionEntityBuilder();
    const xmlNode = loadFixture('function-params.xml');
    
    const entity = builder.build(xmlNode);
    
    expect(entity.parameters.length).toBeGreaterThan(0);
    expect(entity.parameters[0].name).toBeTruthy();
    expect(entity.parameters[0].type).toBeTruthy();
  });
  
  test('identifies virtual methods', () => {
    const builder = new FunctionEntityBuilder();
    const xmlNode = loadFixture('method-virtual.xml');
    const parent = { kind: 'class', qualifiedName: 'MyClass' } as any;
    
    const entity = builder.build(xmlNode, parent);
    
    expect(entity.kind).toBe('method');
    expect(entity.isVirtual).toBe(true);
    expect(entity.parentClass).toBe('MyClass');
  });
});
```

### Integration Tests

```typescript
describe('Parser Integration', () => {
  test('parses real Doxygen output', async () => {
    // Use actual Doxygen XML from a small C++ project
    const result = await parseDoxygenXML('./test-data/real-project/xml');
    
    expect(result.entities.length).toBeGreaterThan(0);
    expect(result.metadata.doxygenVersion).toBeTruthy();
  });
  
  test('handles malformed XML gracefully', async () => {
    const result = await parseDoxygenXML('./test-data/malformed');
    
    // Should not throw, but should log errors
    expect(result.errors.length).toBeGreaterThan(0);
  });
  
  test('handles large projects', async () => {
    const start = performance.now();
    const result = await parseDoxygenXML('./test-data/large-project/xml');
    const duration = performance.now() - start;
    
    expect(result.entities.length).toBeGreaterThan(1000);
    expect(duration).toBeLessThan(5000); // Should parse in under 5 seconds
  });
});
```

### Test Fixtures

```
tests/fixtures/doxygen-xml/
├── simple/              # Basic class and function
│   ├── index.xml
│   ├── classMyClass.xml
│   └── MyClass_8h.xml
├── inheritance/         # Class hierarchies
│   ├── index.xml
│   ├── classBase.xml
│   └── classDerived.xml
├── templates/           # Template classes
│   ├── index.xml
│   └── classVector.xml
├── namespaces/          # Nested namespaces
│   ├── index.xml
│   ├── namespacestd.xml
│   └── namespacedetail.xml
├── visibility/          # Public/private/protected
│   ├── index.xml
│   └── classWithVisibility.xml
└── malformed/           # Invalid XML for error testing
    ├── index.xml
    └── broken.xml
```

## Dependencies

### Required
- `fast-xml-parser` (^4.3.0) - Fast XML to JavaScript object conversion

### Optional (Phase 2)
- `glob` (^10.0.0) - File pattern matching for advanced file discovery
- `crypto` (built-in) - For cache checksums

## Performance Considerations

### Phase 1: Basic Implementation
- Sequential file parsing
- Load all XML into memory
- No caching
- Acceptable for projects with <5k entities

### Phase 2: Optimizations
```typescript
// Parallel file parsing
private async parseCompoundsParallel(
  xmlPath: string,
  compounds: CompoundInfo[]
): Promise<DoxygenEntity[]> {
  const chunkSize = 10;
  const results: DoxygenEntity[] = [];
  
  for (let i = 0; i < compounds.length; i += chunkSize) {
    const chunk = compounds.slice(i, i + chunkSize);
    const chunkResults = await Promise.all(
      chunk.map(c => this.parseCompoundFile(xmlPath, c))
    );
    results.push(...chunkResults.flat());
  }
  
  return results;
}

// Stream-based parsing for very large files
private async parseCompoundStream(
  filePath: string
): Promise<DoxygenEntity[]> {
  // Use streaming XML parser
  // Build entities incrementally
}
```

### Caching Strategy (Phase 2)

```typescript
// cache/cache-manager.ts
export class CacheManager {
  private cachePath: string;
  
  async loadCache(xmlPath: string): Promise<ParseResult | null> {
    // 1. Check if cache file exists
    // 2. Validate cache is not stale
    // 3. Load and return cached result
  }
  
  async saveCache(xmlPath: string, result: ParseResult): Promise<void> {
    // 1. Serialize result
    // 2. Write to cache file
  }
  
  private isStale(cache: CachedResult): boolean {
    // Check if XML files have been modified since cache creation
    // Compare checksums or modification times
  }
}
```

## Error Handling

### Fatal Errors (throw)
- XML directory not found
- index.xml missing
- Invalid XML structure in critical files

### Non-Fatal Errors (log and continue)
- Malformed compound XML
- Missing optional fields
- Invalid references
- Parse errors in individual members

```typescript
class ParserError extends Error {
  constructor(
    message: string,
    public file: string,
    public entityId?: string
  ) {
    super(message);
    this.name = 'ParserError';
  }
}
```

## Design Principles

1. **Robustness**: Handle malformed XML gracefully
2. **Performance**: Optimize for common case (medium projects)
3. **Type Safety**: Leverage TypeScript for compile-time validation
4. **Extensibility**: Easy to add new entity types
5. **Single Responsibility**: Each module has one clear purpose
6. **Pure Functions**: Builders and extractors are stateless

## Public API (`index.ts`)

```typescript
// Main parser
export { DoxygenParser, parseDoxygenXML } from './doxygen-parser';

// Types
export type {
  DoxygenEntity,
  ClassEntity,
  FunctionEntity,
  NamespaceEntity,
  EnumEntity,
  EntityKind,
  Parameter,
  BaseClass,
  ParseResult,
  ParseOptions,
  ParseMetadata,
  ParseError
} from './types';

// Utilities (for advanced users)
export { findDoxygenFiles, readXMLFile } from './xml-reader';
export { EntityBuilderFactory } from './entity-builders/factory';
```

## Future Enhancements

### Phase 2
- Parallel XML parsing for performance
- Persistent cache with invalidation
- Incremental parsing (only changed files)
- XML streaming for very large files

### Phase 3
- Support for custom Doxygen tags
- Cross-reference resolution
- Diagram/graph extraction
- Source code snippet extraction
- Support for other XML formats (JavaDoc, etc.)

## XML Structure Reference

### Common Doxygen XML Patterns

```xml
<!-- index.xml -->
<doxygenindex version="1.9.5">
  <compound refid="class_my_class" kind="class">
    <name>MyClass</name>
    <member refid="class_my_class_1a123" kind="function">
      <name>doWork</name>
    </member>
  </compound>
</doxygenindex>

<!-- classMyClass.xml -->
<doxygen version="1.9.5">
  <compounddef id="class_my_class" kind="class" prot="public">
    <compoundname>MyClass</compoundname>
    <basecompoundref refid="class_base" prot="public">Base</basecompoundref>
    <sectiondef kind="public-func">
      <memberdef kind="function" id="class_my_class_1a123" prot="public">
        <name>doWork</name>
        <type>void</type>
        <param>
          <type>int</type>
          <declname>value</declname>
        </param>
        <briefdescription>
          <para>Does some work</para>
        </briefdescription>
        <location file="myclass.h" line="42"/>
      </memberdef>
    </sectiondef>
  </compounddef>
</doxygen>
```

This parser plan provides a complete, production-ready foundation for Phase 1 with clear paths to Phase 2 and 3 enhancements.