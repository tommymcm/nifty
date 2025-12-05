import Fuse from 'fuse.js';
import { SearchEngine } from './base';
import { SearchQuery, SearchResult, SearchStats } from '../types';
import { DoxygenEntity } from '../../parser/types';

export class FuzzySearchEngine extends SearchEngine {
  private fuse: Fuse<DoxygenEntity> | null = null;
  private stats: SearchStats;
  
  protected onEntitiesLoaded(): void {
    // Build fuzzy search index
    this.fuse = new Fuse(this.entities, {
      keys: [
        { name: 'name', weight: 0.5 },
        { name: 'qualifiedName', weight: 0.3 },
        { name: 'briefDescription', weight: 0.2 }
      ],
      threshold: this.options.fuzzyThreshold || 0.4,
      distance: 100,
      includeScore: true,
      includeMatches: true,
      ignoreLocation: true,
      useExtendedSearch: false
    });
  }
  
  search(query: SearchQuery): SearchResult[] {
    if (!this.fuse) {
      throw new Error('Entities not loaded. Call loadEntities() first.');
    }
    
    const startTime = performance.now();
    
    // Execute fuzzy search
    const fuseResults = this.fuse.search(query.term);
    
    // Convert to our result format
    let results: SearchResult[] = fuseResults.map(result => ({
      entity: result.item,
      relevance: 1 - (result.score || 0), // Fuse score is 0=perfect, 1=worst
      matchedFields: (result.matches || []).map(m => ({
        fieldName: m.key || 'unknown',
        matchType: 'fuzzy' as const,
        score: 1 - (m.score || 0)
      })),
      highlights: this.options.enableHighlights
        ? this.convertMatches(result.matches || [])
        : undefined
    }));
    
    // Apply type filter if specified
    if (query.entityTypes && query.entityTypes.length > 0) {
      results = results.filter(r => 
        query.entityTypes!.includes(r.entity.kind)
      );
    }
    
    // Apply result limit
    const maxResults = query.maxResults || this.options.maxResults;
    results = results.slice(0, maxResults);
    
    this.stats = {
      totalEntities: this.entities.length,
      matchedEntities: fuseResults.length,
      returnedResults: results.length,
      searchTimeMs: performance.now() - startTime,
      cacheHit: false
    };
    
    return results;
  }
  
  getStats(): SearchStats {
    return this.stats;
  }
  
  private convertMatches(matches: Fuse.FuseResultMatch[]): Highlight[] {
    return matches.flatMap(match => 
      (match.indices || []).map(([start, end]) => ({
        field: match.key || 'unknown',
        start,
        length: end - start + 1
      }))
    );
  }
}