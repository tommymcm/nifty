
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