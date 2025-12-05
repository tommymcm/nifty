# Search Directory Plan

## Purpose
Implements search algorithms and result processing for querying Doxygen entities. This is the core logic layer that takes parsed entities and returns relevant, ranked results. Designed with a pluggable architecture to support multiple search strategies (simple, fuzzy, AI-assisted).

## Structure
```
search/
├── index.ts               # Public API exports
├── types.ts               # Search-specific type definitions
├── engines/
│   ├── base.ts           # Abstract SearchEngine interface
│   ├── simple.ts         # String-based search (Phase 1)
│   ├── fuzzy.ts          # Fuzzy matching (Phase 2)
│   └── ai.ts             # AI-assisted search (Phase 3)
├── filters/
│   ├── base.ts           # Filter interface
│   ├── type-filter.ts    # Filter by entity type
│   ├── visibility-filter.ts  # Filter by public/private/protected
│   ├── namespace-filter.ts   # Filter by namespace
│   └── file-filter.ts    # Filter by file path
├── rankers/
│   ├── base.ts           # Ranker interface
│   ├── relevance.ts      # Default relevance scoring
│   ├── recency.ts        # Boost recent entities (Phase 2)
│   └── context.ts        # Boost contextually relevant (Phase 2)
├── formatters/
│   ├── base.ts           # Formatter interface
│   ├── text.ts           # Plain text output
│   ├── json.ts           # JSON output
│   ├── table.ts          # Table format (Phase 2)
│   └── detailed.ts       # Verbose output (Phase 2)
└── utils/
    ├── highlighter.ts    # Text highlighting utilities
    └── scorer.ts         # Scoring utilities
```

## Core Type Definitions (`types.ts`)

```typescript
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
```

## Search Engine Architecture

### Base Interface (`engines/base.ts`)

```typescript
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
```

### Simple Search Engine (`engines/simple.ts`)

**Phase 1 Implementation** - Core string-based searching

```typescript
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
```

### Fuzzy Search Engine (`engines/fuzzy.ts`)

**Phase 2 Implementation** - Typo-tolerant searching

```typescript
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
```

### AI Search Engine (`engines/ai.ts`)

**Phase 3 Implementation** - Semantic search with AI

```typescript
import { SearchEngine } from './base';
import { SearchQuery, SearchResult, SearchStats } from '../types';

export class AISearchEngine extends SearchEngine {
  private stats: SearchStats;
  
  search(query: SearchQuery): SearchResult[] {
    // Placeholder for Phase 3
    // Will implement:
    // 1. Generate embeddings for entities
    // 2. Generate embedding for query
    // 3. Compute semantic similarity
    // 4. Return ranked results
    
    throw new Error('AI search not yet implemented (Phase 3)');
  }
  
  getStats(): SearchStats {
    return this.stats;
  }
}
```

## Filters (`filters/`)

### Base Filter (`filters/base.ts`)

```typescript
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
```

### Type Filter (`filters/type-filter.ts`)

```typescript
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
```

### Namespace Filter (`filters/namespace-filter.ts`)

```typescript
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
```

## Formatters (`formatters/`)

### Base Formatter (`formatters/base.ts`)

```typescript
import { SearchResult } from '../types';

/**
 * Interface for result formatters
 */
export interface ResultFormatter {
  format(results: SearchResult[], stats?: SearchStats): string;
}

/**
 * Factory for creating formatters
 */
export class FormatterFactory {
  static create(format: 'text' | 'json' | 'table' | 'detailed'): ResultFormatter {
    switch (format) {
      case 'text':
        return new TextFormatter();
      case 'json':
        return new JSONFormatter();
      case 'table':
        return new TableFormatter();
      case 'detailed':
        return new DetailedFormatter();
      default:
        throw new Error(`Unknown format: ${format}`);
    }
  }
}
```

### Text Formatter (`formatters/text.ts`)

```typescript
import { ResultFormatter } from './base';
import { SearchResult, SearchStats } from '../types';
import chalk from 'chalk';

export class TextFormatter implements ResultFormatter {
  format(results: SearchResult[], stats?: SearchStats): string {
    if (results.length === 0) {
      return chalk.dim('No results found.');
    }
    
    const lines: string[] = [];
    
    // Header
    lines.push(chalk.bold(`Found ${results.length} result(s):\n`));
    
    // Format each result
    results.forEach((result, index) => {
      lines.push(this.formatResult(result, index + 1));
      lines.push(''); // Blank line between results
    });
    
    // Stats if provided
    if (stats) {
      lines.push(chalk.dim(this.formatStats(stats)));
    }
    
    return lines.join('\n');
  }
  
  private formatResult(result: SearchResult, index: number): string {
    const entity = result.entity;
    const lines: string[] = [];
    
    // 1. Result number and name
    const nameColor = this.getKindColor(entity.kind);
    lines.push(
      `${chalk.cyan(`${index}.`)} ${nameColor(entity.name)} ` +
      chalk.dim(`(${entity.kind})`)
    );
    
    // 2. Location
    lines.push(
      `   ${chalk.dim('File:')} ${entity.file}:${chalk.yellow(entity.line)}`
    );
    
    // 3. Relevance score
    const scorePercent = Math.round(result.relevance * 100);
    lines.push(
      `   ${chalk.dim('Match:')} ${this.formatScore(scorePercent)}`
    );
    
    // 4. Brief description if available
    if (entity.briefDescription) {
      const desc = this.truncate(entity.briefDescription, 80);
      lines.push(`   ${chalk.dim(desc)}`);
    }
    
    // 5. Matched fields
    if (result.matchedFields.length > 0) {
      const fields = result.matchedFields
        .map(f => f.fieldName)
        .join(', ');
      lines.push(`   ${chalk.dim('Matched in:')} ${fields}`);
    }
    
    return lines.join('\n');
  }
  
  private getKindColor(kind: string): (s: string) => string {
    switch (kind) {
      case 'class':
      case 'struct':
        return chalk.green;
      case 'function':
      case 'method':
        return chalk.blue;
      case 'namespace':
        return chalk.magenta;
      default:
        return chalk.white;
    }
  }
  
  private formatScore(percent: number): string {
    if (percent >= 90) return chalk.green(`${percent}%`);
    if (percent >= 70) return chalk.yellow(`${percent}%`);
    return chalk.red(`${percent}%`);
  }
  
  private formatStats(stats: SearchStats): string {
    return [
      `Searched ${stats.totalEntities} entities`,
      `in ${stats.searchTimeMs.toFixed(2)}ms`,
      stats.cacheHit ? '(cached)' : ''
    ].filter(Boolean).join(' ');
  }
  
  private truncate(text: string, maxLength: number): string {
    if (text.length <= maxLength) return text;
    return text.substring(0, maxLength - 3) + '...';
  }
}
```

### JSON Formatter (`formatters/json.ts`)

```typescript
import { ResultFormatter } from './base';
import { SearchResult, SearchStats } from '../types';

export class JSONFormatter implements ResultFormatter {
  format(results: SearchResult[], stats?: SearchStats): string {
    const output = {
      total: results.length,
      results: results.map(r => ({
        name: r.entity.name,
        qualifiedName: r.entity.qualifiedName,
        kind: r.entity.kind,
        file: r.entity.file,
        line: r.entity.line,
        relevance: r.relevance,
        briefDescription: r.entity.briefDescription,
        matchedFields: r.matchedFields.map(f => ({
          field: f.fieldName,
          type: f.matchType,
          score: f.score
        }))
      })),
      ...(stats && { stats })
    };
    
    return JSON.stringify(output, null, 2);
  }
}
```

## Utilities

### Scorer (`utils/scorer.ts`)

```typescript
/**
 * Normalize a string for comparison
 */
export function normalizeString(s: string, caseSensitive: boolean): string {
  return caseSensitive ? s : s.toLowerCase();
}

/**
 * Calculate Levenshtein distance between two strings
 */
export function levenshteinDistance(a: string, b: string): number {
  const matrix: number[][] = [];
  
  for (let i = 0; i <= b.length; i++) {
    matrix[i] = [i];
  }
  
  for (let j = 0; j <= a.length; j++) {
    matrix[0][j] = j;
  }
  
  for (let i = 1; i <= b.length; i++) {
    for (let j = 1; j <= a.length; j++) {
      if (b.charAt(i - 1) === a.charAt(j - 1)) {
        matrix[i][j] = matrix[i - 1][j - 1];
      } else {
        matrix[i][j] = Math.min(
          matrix[i - 1][j - 1] + 1,
          matrix[i][j - 1] + 1,
          matrix[i - 1][j] + 1
        );
      }
    }
  }
  
  return matrix[b.length][a.length];
}

/**
 * Calculate similarity score (0-1) based on Levenshtein distance
 */
export function similarityScore(a: string, b: string): number {
  const distance = levenshteinDistance(a, b);
  const maxLength = Math.max(a.length, b.length);
  return maxLength === 0 ? 1 : 1 - (distance / maxLength);
}
```

### Highlighter (`utils/highlighter.ts`)

```typescript
import chalk from 'chalk';
import { Highlight } from '../types';

/**
 * Highlight matched text in a string
 */
export function highlightText(
  text: string,
  highlights: Highlight[],
  color: (s: string) => string = chalk.cyan.bold
): string {
  if (!highlights || highlights.length === 0) {
    return text;
  }
  
  // Sort highlights by start position
  const sorted = [...highlights].sort((a, b) => a.start - b.start);
  
  let result = '';
  let lastEnd = 0;
  
  for (const highlight of sorted) {
    // Add text before highlight
    result += text.substring(lastEnd, highlight.start);
    
    // Add highlighted text
    const highlightedText = text.substring(
      highlight.start,
      highlight.start + highlight.length
    );
    result += color(highlightedText);
    
    lastEnd = highlight.start + highlight.length;
  }
  
  // Add remaining text
  result += text.substring(lastEnd);
  
  return result;
}

/**
 * Create a context snippet with highlighted matches
 */
export function createSnippet(
  text: string,
  highlight: Highlight,
  contextLength: number = 40
): string {
  const start = Math.max(0, highlight.start - contextLength);
  const end = Math.min(
    text.length,
    highlight.start + highlight.length + contextLength
  );
  
  let snippet = text.substring(start, end);
  
  if (start > 0) snippet = '...' + snippet;
  if (end < text.length) snippet = snippet + '...';
  
  return snippet;
}
```

## Public API (`index.ts`)

```typescript
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

// Filters
export { ResultFilter, FilterChain } from './filters/base';
export { TypeFilter } from './filters/type-filter';
export { NamespaceFilter } from './filters/namespace-filter';

// Formatters
export { ResultFormatter, FormatterFactory } from './formatters/base';
export { TextFormatter } from './formatters/text';
export { JSONFormatter } from './formatters/json';
```

## Testing Strategy

### Unit Tests

```typescript
describe('SimpleSearchEngine', () => {
  let engine: SimpleSearchEngine;
  let mockEntities: DoxygenEntity[];
  
  beforeEach(() => {
    engine = new SimpleSearchEngine();
    mockEntities = createMockEntities();
    engine.loadEntities(mockEntities);
  });
  
  test('exact match returns highest score', () => {
    const results = engine.search({ term: 'MyClass' });
    expect(results[0].relevance).toBe(1.0);
  });
  
  test('prefix match scores higher than substring', () => {
    const results = engine.search({ term: 'My' });
    const prefixResult = results.find(r => r.entity.name === 'MyClass');
    const substringResult = results.find(r => r.entity.name === 'GetMyValue');
    expect(prefixResult!.relevance).toBeGreaterThan(substringResult!.relevance);
  });
  
  test('case insensitive by default', () => {
    const results = engine.search({ term: 'myclass' });
    expect(results.length).toBeGreaterThan(0);
  });
  
  test('respects maxResults limit', () => {
    const results = engine.search({ term: 'test', maxResults: 5 });
    expect(results.length).toBeLessThanOrEqual(5);
  });
});

describe('TypeFilter', () => {
  test('filters by entity type', () => {
    const filter = new TypeFilter(['class']);
    const results = createMockResults();
    const filtered = filter.apply(results);
    expect(filtered.every(r => r.entity.kind === 'class')).toBe(true);
  });
});

describe('TextFormatter', () => {
  test('formats results as text', () => {
    const formatter = new TextFormatter();
    const results = createMockResults();
    const output = formatter.format(results);
    expect(output).toContain('Found');
    expect(output).toContain('result');
  });
});
```

### Performance Tests

```typescript
describe('Search Performance', () => {
  test('searches 10k entities in under 100ms', () => {
    const entities = generateMockEntities(10000);
    const engine = new SimpleSearchEngine();
    engine.loadEntities(entities);
    
    const start = performance.now();
    const results = engine.search({ term: 'test' });
    const duration = performance.now() - start;
    
    expect(duration).toBeLessThan(100);
  });
  
  test('fuzzy search scales well', () => {
    const entities = generateMockEntities(5000);
    const engine = new FuzzySearchEngine();
    engine.loadEntities(entities);
    
    const start = performance.now();
    const results = engine.search({ term: 'tesst' }); // typo
    const duration = performance.now() - start;
    
    expect(duration).toBeLessThan(200);
  });
});
```

## Dependencies

### Phase 1 (Required)
- `chalk` (^5.0.0) - Terminal colors for text formatter

### Phase 2
- `fuse.js` (^7.0.0) - Fuzzy search implementation
- `cli-table3` (^0.6.0) - Table formatter

### Phase 3
- `@anthropic-ai/sdk` - AI API client
- Vector similarity library (TBD)

## Design Principles

1. **Separation of Concerns**: Search, filter, rank, and format are independent
2. **Strategy Pattern**: Easy to swap search algorithms
3. **Composability**: Filters and rankers can be chained
4. **Performance First**: Optimize for common case (10k entities, <100ms)
5. **Pure Functions**: Search logic is stateless and testable
6. **Progressive Enhancement**: Basic features work without optional dependencies

## Performance Optimizations

### Phase 1
- String operations optimized for common case
- Early termination for exact matches
- Minimal object allocations
- Result limit applied early

### Phase 2
- Pre-built indexes for fuzzy search
- Result caching based on query hash
- Lazy evaluation of descriptions
- Parallel scoring for large entity sets

### Phase 3
- Persistent embedding cache
- Vector index for fast similarity search
- Batch processing for AI requests
- Incremental updates for changed entities

## Future Enhancements

- **Regular Expression Search**: Pattern-based matching
- **Boolean Queries**: AND, OR, NOT operators
- **Field-Specific Search**: Search only in names, only in descriptions
- **Wildcard Support**: `*` and `?` patterns
- **Phrase Search**: Exact phrase matching with quotes
- **Search Suggestions**: "Did you mean?" functionality
- **Result Clustering**: Group related results
- **Custom Scoring**: User-defined relevance functions
- **Search History**: Track and suggest previous queries</parameter>