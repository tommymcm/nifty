import { BaseClass } from '../types';

/**
 * Extract base classes from class XML node
 */
export function extractBaseClasses(xmlNode: any): BaseClass[] {
  const baseCompoundRefs = xmlNode.basecompoundref;
  if (!baseCompoundRefs) return [];
  
  const refs = Array.isArray(baseCompoundRefs)
    ? baseCompoundRefs
    : [baseCompoundRefs];
  
  return refs
    .filter(Boolean)
    .map(ref => ({
      name: extractText(ref),
      qualifiedName: ref['@_refid'] || extractText(ref),
      visibility: (ref['@_prot'] || 'public') as any,
      isVirtual: ref['@_virt'] === 'virtual'
    }));
}

function extractText(node: any): string {
  if (typeof node === 'string') return node;
  if (node['#text']) return node['#text'];
  return '';
}