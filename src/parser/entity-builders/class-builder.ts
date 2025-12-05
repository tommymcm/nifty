import { EntityBuilder } from './base';
import { ClassEntity, BaseClass, MemberReference } from '../types';
import { extractBaseClasses, extractMembers } from '../extractors';

export class ClassEntityBuilder extends EntityBuilder<ClassEntity> {
  build(xmlNode: any): ClassEntity {
    const base = this.buildBase(xmlNode);
    const kind = xmlNode['@_kind'] as 'class' | 'struct';
    
    return {
      ...base,
      kind,
      baseClasses: extractBaseClasses(xmlNode),
      members: extractMembers(xmlNode),
      templateParams: this.extractTemplateParams(xmlNode),
      isAbstract: this.checkIsAbstract(xmlNode),
      namespace: this.extractNamespace(base.qualifiedName)
    } as ClassEntity;
  }
  
  private extractTemplateParams(xmlNode: any): any[] | undefined {
    const templateParamList = xmlNode.templateparamlist;
    if (!templateParamList?.param) return undefined;
    
    const params = Array.isArray(templateParamList.param)
      ? templateParamList.param
      : [templateParamList.param];
    
    return params.map(p => ({
      name: p.declname || '',
      type: p.type || '',
      defaultValue: p.defval
    }));
  }
  
  private checkIsAbstract(xmlNode: any): boolean {
    // Check if any member is pure virtual
    const sections = Array.isArray(xmlNode.sectiondef)
      ? xmlNode.sectiondef
      : xmlNode.sectiondef ? [xmlNode.sectiondef] : [];
    
    for (const section of sections) {
      const members = Array.isArray(section.memberdef)
        ? section.memberdef
        : section.memberdef ? [section.memberdef] : [];
      
      for (const member of members) {
        if (member['@_virt'] === 'pure-virtual') {
          return true;
        }
      }
    }
    
    return false;
  }
  
  private extractNamespace(qualifiedName: string): string | undefined {
    const lastColon = qualifiedName.lastIndexOf('::');
    if (lastColon === -1) return undefined;
    return qualifiedName.substring(0, lastColon);
  }
}