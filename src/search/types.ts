import { DoxygenEntity, EntityKind } from '../parser/types';

/**
 * Search query with all parameters
 */
export interface SearchQuery {
  term: string;                    // Search term
  entityTypes?: EntityKind[];      // Filter by entity types
  caseSensitive?: boolean;         // Case-sensitive matching
  exactMatch?: boolean;            // Exact vs substring match
  maxResults?: number;             // Limit result count
  namespace?: string;              // Filter by namespace
  filePattern?: string;            // Filter by file pattern
}

/**
 * Search result with scoring and metadata
 */
export interface SearchResult {
  entity: DoxygenEntity;           // The matched entity
  relevance: number;               // Relevance score (0-1)
  matchedFields: MatchedField[];   // Which fields matched
  highlights?: Highlight[];        // Match positions for highlighting
}

/**
 * Field that matched the query
 */
export interface MatchedField {
  fieldName: string;               // 'name', 'qualifiedName', 'description'
  matchType: MatchType;            // How it matched
  score: number;                   // Contribution to total score
}

export type MatchType = 
  | 'exact'                        // Exact match
  | 'prefix'                       // Starts with term
  | 'substring'                    // Contains term
  | 'fuzzy'                        // Fuzzy match
  | 'semantic';                    // Semantic similarity

/**
 * Highlight position for displaying matches
 */
export interface Highlight {
  field: string;                   // Which field
  start: number;                   // Start position
  length: number;                  // Length of match
  context?: string;                // Surrounding context
}

/**
 * Search engine configuration
 */
export interface SearchOptions {
  // Engine behavior
  caseSensitive: boolean;
  exactMatch: boolean;
  fuzzyThreshold?: number;         // 0-1, for fuzzy search
  maxResults: number;
  
  // Features
  enableHighlights: boolean;
  includeDescriptions: boolean;
  
  // Performance
  cacheResults: boolean;
  timeoutMs?: number;
}

/**
 * Search statistics
 */
export interface SearchStats {
  totalEntities: number;           // Total entities searched
  matchedEntities: number;         // Entities with any match
  returnedResults: number;         // Results after filtering/limiting
  searchTimeMs: number;            // Time taken
  cacheHit: boolean;               // Was cache used
}