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