import { MemberReference } from '../types';

/**
 * Extract member references from class XML node
 */
export function extractMembers(xmlNode: any): MemberReference[] {
  const members: MemberReference[] = [];
  
  const sections = Array.isArray(xmlNode.sectiondef)
    ? xmlNode.sectiondef
    : xmlNode.sectiondef ? [xmlNode.sectiondef] : [];
  
  for (const section of sections) {
    const memberDefs = Array.isArray(section.memberdef)
      ? section.memberdef
      : section.memberdef ? [section.memberdef] : [];
    
    for (const memberDef of memberDefs) {
      members.push({
        id: memberDef['@_id'] || '',
        name: memberDef.name || '',
        kind: memberDef['@_kind'] as any,
        visibility: (memberDef['@_prot'] || 'public') as any
      });
    }
  }
  
  return members;
}