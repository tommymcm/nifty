import { SearchEngine } from './base';
import { SearchQuery, SearchResult, SearchStats, MatchedField } from '../types';
import { DoxygenEntity } from '../../parser/types';
import { scoreMatch, normalizeString } from '../utils/scorer';

export class SimpleSearchEngine extends SearchEngine {
  private stats: SearchStats;
  
  search(query: SearchQuery): SearchResult[] {
    const startTime = performance.now();
    
    // 1. Normalize search term
    const searchTerm = this.options.caseSensitive 
      ? query.term 
      : query.term.toLowerCase();
    
    // 2. Filter entities by type if specified
    let candidateEntities = this.entities;
    if (query.entityTypes && query.entityTypes.length > 0) {
      candidateEntities = candidateEntities.filter(e => 
        query.entityTypes!.includes(e.kind)
      );
    }
    
    // 3. Score each entity
    const scoredResults: SearchResult[] = [];
    for (const entity of candidateEntities) {
      const result = this.scoreEntity(entity, searchTerm, query);
      if (result && result.relevance > 0) {
        scoredResults.push(result);
      }
    }
    
    // 4. Sort by relevance (highest first)
    scoredResults.sort((a, b) => b.relevance - a.relevance);
    
    // 5. Apply result limit
    const maxResults = query.maxResults || this.options.maxResults;
    const results = scoredResults.slice(0, maxResults);
    
    // 6. Record statistics
    this.stats = {
      totalEntities: this.entities.length,
      matchedEntities: scoredResults.length,
      returnedResults: results.length,
      searchTimeMs: performance.now() - startTime,
      cacheHit: false
    };
    
    return results;
  }
  
  getStats(): SearchStats {
    return this.stats;
  }
  
  /**
   * Score an entity against the search term
   */
  private scoreEntity(
    entity: DoxygenEntity,
    searchTerm: string,
    query: SearchQuery
  ): SearchResult | null {
    const matchedFields: MatchedField[] = [];
    let totalScore = 0;
    
    // Normalize entity strings for comparison
    const normalize = (s: string) => 
      this.options.caseSensitive ? s : s.toLowerCase();
    
    // Check name field
    const nameScore = this.scoreField(
      normalize(entity.name),
      searchTerm,
      'name',
      query.exactMatch || false
    );
    if (nameScore) {
      matchedFields.push(nameScore);
      totalScore += nameScore.score;
    }
    
    // Check qualified name
    const qualifiedScore = this.scoreField(
      normalize(entity.qualifiedName),
      searchTerm,
      'qualifiedName',
      query.exactMatch || false
    );
    if (qualifiedScore) {
      matchedFields.push(qualifiedScore);
      totalScore += qualifiedScore.score * 0.8; // Slightly lower weight
    }
    
    // Check brief description if enabled
    if (this.options.includeDescriptions && entity.briefDescription) {
      const descScore = this.scoreField(
        normalize(entity.briefDescription),
        searchTerm,
        'briefDescription',
        false // Never exact match descriptions
      );
      if (descScore) {
        matchedFields.push(descScore);
        totalScore += descScore.score * 0.3; // Much lower weight
      }
    }
    
    // No matches found
    if (matchedFields.length === 0) {
      return null;
    }
    
    // Normalize total score to 0-1 range
    const relevance = Math.min(totalScore, 1.0);
    
    return {
      entity,
      relevance,
      matchedFields,
      highlights: this.options.enableHighlights 
        ? this.generateHighlights(entity, searchTerm, matchedFields)
        : undefined
    };
  }
  
  /**
   * Score a single field
   */
  private scoreField(
    fieldValue: string,
    searchTerm: string,
    fieldName: string,
    exactMatch: boolean
  ): MatchedField | null {
    // Exact match
    if (fieldValue === searchTerm) {
      return {
        fieldName,
        matchType: 'exact',
        score: 1.0
      };
    }
    
    // If exact match required, stop here
    if (exactMatch) {
      return null;
    }
    
    // Prefix match (starts with)
    if (fieldValue.startsWith(searchTerm)) {
      return {
        fieldName,
        matchType: 'prefix',
        score: 0.9
      };
    }
    
    // Substring match (contains)
    if (fieldValue.includes(searchTerm)) {
      // Score based on position (earlier is better)
      const position = fieldValue.indexOf(searchTerm);
      const positionRatio = 1 - (position / fieldValue.length);
      const score = 0.5 + (positionRatio * 0.3); // 0.5 to 0.8
      
      return {
        fieldName,
        matchType: 'substring',
        score
      };
    }
    
    // No match
    return null;
  }
  
  /**
   * Generate highlight positions for matched text
   */
  private generateHighlights(
    entity: DoxygenEntity,
    searchTerm: string,
    matchedFields: MatchedField[]
  ): Highlight[] {
    const highlights: Highlight[] = [];
    const normalize = (s: string) => 
      this.options.caseSensitive ? s : s.toLowerCase();
    
    for (const match of matchedFields) {
      let fieldValue: string;
      
      switch (match.fieldName) {
        case 'name':
          fieldValue = entity.name;
          break;
        case 'qualifiedName':
          fieldValue = entity.qualifiedName;
          break;
        case 'briefDescription':
          fieldValue = entity.briefDescription || '';
          break;
        default:
          continue;
      }
      
      const normalizedValue = normalize(fieldValue);
      const index = normalizedValue.indexOf(searchTerm);
      
      if (index !== -1) {
        highlights.push({
          field: match.fieldName,
          start: index,
          length: searchTerm.length,
          context: this.getContext(fieldValue, index, searchTerm.length)
        });
      }
    }
    
    return highlights;
  }
  
  /**
   * Get surrounding context for a match
   */
  private getContext(text: string, start: number, length: number): string {
    const contextLength = 40;
    const beforeStart = Math.max(0, start - contextLength);
    const afterEnd = Math.min(text.length, start + length + contextLength);
    
    let context = text.substring(beforeStart, afterEnd);
    
    if (beforeStart > 0) context = '...' + context;
    if (afterEnd < text.length) context = context + '...';
    
    return context;
  }
}