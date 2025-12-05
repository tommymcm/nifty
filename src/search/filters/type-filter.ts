import { ResultFilter } from './base';
import { SearchResult } from '../types';
import { EntityKind } from '../../parser/types';

export class TypeFilter implements ResultFilter {
  constructor(private types: EntityKind[]) {}
  
  apply(results: SearchResult[]): SearchResult[] {
    return results.filter(result => 
      this.types.includes(result.entity.kind)
    );
  }
}