import { readdir, readFile, stat } from 'fs/promises';
import { join } from 'path';

/**
 * Find all Doxygen XML files in directory
 */
export async function findDoxygenFiles(xmlPath: string): Promise<string[]> {
  try {
    const entries = await readdir(xmlPath);
    
    const xmlFiles = entries
      .filter(entry => entry.endsWith('.xml'))
      .map(entry => join(xmlPath, entry));
    
    return xmlFiles;
  } catch (error) {
    throw new Error(`Cannot read XML directory: ${error.message}`);
  }
}

/**
 * Read XML file content
 */
export async function readXMLFile(filePath: string): Promise<string> {
  try {
    return await readFile(filePath, 'utf-8');
  } catch (error) {
    throw new Error(`Cannot read XML file ${filePath}: ${error.message}`);
  }
}

/**
 * Validate that path is a Doxygen XML directory
 */
export async function validateDoxygenPath(xmlPath: string): Promise<void> {
  // Check directory exists
  try {
    const stats = await stat(xmlPath);
    if (!stats.isDirectory()) {
      throw new Error('Path is not a directory');
    }
  } catch (error) {
    throw new Error(`Invalid XML path: ${error.message}`);
  }
  
  // Check for index.xml
  try {
    await stat(join(xmlPath, 'index.xml'));
  } catch (error) {
    throw new Error('index.xml not found - not a valid Doxygen XML directory');
  }
}