interface SearchEngine {
  search(query: string, entities: DoxygenEntity[]): SearchResult[];
}

// Current: Simple string matching
class SimpleSearchEngine implements SearchEngine { ... }
