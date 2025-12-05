import { SearchResult } from '../types';

/**
 * Interface for result filters
 */
export interface ResultFilter {
  apply(results: SearchResult[]): SearchResult[];
}

/**
 * Chain multiple filters together
 */
export class FilterChain implements ResultFilter {
  private filters: ResultFilter[] = [];
  
  add(filter: ResultFilter): this {
    this.filters.push(filter);
    return this;
  }
  
  apply(results: SearchResult[]): SearchResult[] {
    return this.filters.reduce(
      (filtered, filter) => filter.apply(filtered),
      results
    );
  }
}