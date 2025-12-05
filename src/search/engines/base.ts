import { SearchQuery, SearchResult, SearchOptions, SearchStats } from '../types';
import { DoxygenEntity } from '../../parser/types';

/**
 * Abstract search engine interface
 * All search implementations must conform to this
 */
export abstract class SearchEngine {
  protected options: SearchOptions;
  protected entities: DoxygenEntity[] = [];
  
  constructor(options: Partial<SearchOptions> = {}) {
    this.options = this.getDefaultOptions(options);
  }
  
  /**
   * Configure the search engine
   */
  configure(options: Partial<SearchOptions>): void {
    this.options = { ...this.options, ...options };
  }
  
  /**
   * Load entities to search
   */
  loadEntities(entities: DoxygenEntity[]): void {
    this.entities = entities;
    this.onEntitiesLoaded();
  }
  
  /**
   * Execute search query
   */
  abstract search(query: SearchQuery): SearchResult[];
  
  /**
   * Get search statistics from last search
   */
  abstract getStats(): SearchStats;
  
  /**
   * Hook called after entities are loaded
   * Useful for building indexes
   */
  protected onEntitiesLoaded(): void {
    // Override in subclasses if needed
  }
  
  /**
   * Get default options merged with user options
   */
  protected getDefaultOptions(userOptions: Partial<SearchOptions>): SearchOptions {
    return {
      caseSensitive: false,
      exactMatch: false,
      maxResults: 50,
      enableHighlights: true,
      includeDescriptions: true,
      cacheResults: true,
      ...userOptions
    };
  }
}

/**
 * Factory for creating search engines
 */
export class SearchEngineFactory {
  static create(
    type: 'simple' | 'fuzzy' | 'ai',
    options?: Partial<SearchOptions>
  ): SearchEngine {
    switch (type) {
      case 'simple':
        return new SimpleSearchEngine(options);
      case 'fuzzy':
        return new FuzzySearchEngine(options);
      case 'ai':
        return new AISearchEngine(options);
      default:
        throw new Error(`Unknown search engine type: ${type}`);
    }
  }
}