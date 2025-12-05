import { ResultFilter } from './base';
import { SearchResult } from '../types';

export class NamespaceFilter implements ResultFilter {
  constructor(private namespace: string) {}
  
  apply(results: SearchResult[]): SearchResult[] {
    return results.filter(result => 
      result.entity.qualifiedName.startsWith(this.namespace + '::') ||
      result.entity.qualifiedName === this.namespace
    );
  }
}