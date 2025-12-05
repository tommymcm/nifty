import { parseDoxygenXML } from '../../parser';
import { SearchEngineFactory } from '../../search';
import { FormatterFactory } from '../../search/formatters';
import { loadConfig, mergeConfig } from '../config';
import { validateSearchQuery, validatePath } from '../validators';
import { showSpinner, formatResults } from '../output';

export async function searchCommand(
  query: string,
  options: SearchCommandOptions
): Promise<void> {
  // 1. Load and merge configuration
  const config = await loadConfig(options.config);
  const finalConfig = mergeConfig(config, options);
  
  // 2. Validate inputs
  validateSearchQuery(query);
  validatePath(finalConfig.path);
  
  // 3. Parse Doxygen XML
  const spinner = showSpinner('Parsing Doxygen XML...');
  let parsed;
  try {
    parsed = await parseDoxygenXML(finalConfig.path, {
      useCache: true,
      entityTypes: finalConfig.type === 'all' ? undefined : [finalConfig.type]
    });
    spinner.succeed(`Found ${parsed.entities.length} entities`);
  } catch (error) {
    spinner.fail('Failed to parse Doxygen XML');
    throw new ParseError(`Cannot parse XML: ${error.message}`);
  }
  
  // 4. Execute search
  const searchEngine = SearchEngineFactory.create('simple');
  searchEngine.configure({
    caseSensitive: !finalConfig.caseInsensitive,
    exactMatch: finalConfig.exact,
    maxResults: parseInt(finalConfig.limit)
  });
  
  const results = searchEngine.search({
    term: query,
    entityTypes: finalConfig.type === 'all' ? undefined : [finalConfig.type]
  });
  
  // 5. Format and display results
  const formatter = FormatterFactory.create(finalConfig.format);
  const output = formatter.format(results);
  
  console.log(output);
  
  // 6. Show summary
  if (finalConfig.verbose) {
    console.log(`\nSearch completed in ${elapsed}ms`);
    console.log(`Total entities scanned: ${parsed.entities.length}`);
    console.log(`Results returned: ${results.length}`);
  }
  
  // Exit with appropriate code
  if (results.length === 0) {
    console.log('\nNo results found. Try:');
    console.log('  - Checking your spelling');
    console.log('  - Using a broader search term');
    console.log('  - Removing the --exact flag');
  }
}

interface SearchCommandOptions {
  path?: string;
  format?: string;
  verbose?: boolean;
  config?: string;
  type?: string;
  exact?: boolean;
  caseInsensitive?: boolean;
  limit?: string;
}