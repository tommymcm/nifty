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