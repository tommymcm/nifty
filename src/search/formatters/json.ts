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