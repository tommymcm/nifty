// Engines
export { SearchEngine, SearchEngineFactory } from './engines/base';
export { SimpleSearchEngine } from './engines/simple';

// Types
export type {
  SearchQuery,
  SearchResult,
  SearchOptions,
  SearchStats,
  MatchedField,
  Highlight
} from './types';
