import { Parameter } from '../types';

/**
 * Extract function parameters from XML node
 */
export function extractParameters(xmlNode: any): Parameter[] {
  const params = xmlNode.param;
  if (!params) return [];
  
  const paramList = Array.isArray(params) ? params : [params];
  
  return paramList
    .filter(Boolean)
    .map(param => ({
      name: param.declname || param.defname || '',
      type: extractType(param.type),
      defaultValue: param.defval ? extractText(param.defval) : undefined,
      description: undefined // TODO: Extract from param documentation
    }));
}

function extractType(typeNode: any): string {
  if (!typeNode) return '';
  return extractText(typeNode);
}

function extractText(node: any): string {
  if (typeof node === 'string') return node;
  if (node['#text']) return node['#text'];
  if (typeof node === 'object') {
    return Object.values(node)
      .map(v => extractText(v))
      .join('')
      .trim();
  }
  return '';
}