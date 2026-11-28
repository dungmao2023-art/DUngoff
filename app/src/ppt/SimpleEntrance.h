// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QSet>
namespace slideshow {
// Bounded sequence of instant, whole-shape entrances, one per click.
// Unknown/timed/interactive trees are never approximated as instant effects.
inline bool findClickEntrances(const QJsonObject& root,QStringList* targets){
    if(!targets)return false;
    targets->clear();
    QStringList candidate; QSet<QString> seen;
    int clicks=0,nodes=0;bool valid=true;
    auto visit=[&](auto&& self,const QJsonObject& node,int depth,int click)->void{
        if(!valid||depth>32||++nodes>128){valid=false;return;}
        const auto type=node.value("nodeType").toString(),kind=node.value("nodeName").toString();
        if(type=="InteractiveSequence"||type=="WithPrevious"||type=="AfterPrevious"||
           node.contains("repeatCount")||node.contains("autoreverse")){valid=false;return;}
        if(type=="OnClick"){
            if(click!=0||++clicks>8){valid=false;return;}
            click=clicks;
        }
        const auto children=node.value("children");
        if(kind=="Par"||kind=="Seq"){
            if(!children.isArray()||children.toArray().isEmpty()){valid=false;return;}
            for(const auto& child:children.toArray()){
                if(!child.isObject()){valid=false;return;}
                self(self,child.toObject(),depth+1,click);
            }
            return;
        }
        const auto name=node.value("attributeName").toString(),value=node.value("to").toString().toLower();
        const auto sub=node.value("subItem").toString(),target=node.value("targetElement").toString();
        if(click!=candidate.size()+1||kind!="Set"||name!="Visibility"||(value!="visible"&&value!="true")||
           (!sub.isEmpty()&&sub!="AsWhole")||target.isEmpty()||seen.contains(target)){valid=false;return;}
        candidate.append(target);seen.insert(target);
    };
    visit(visit,root,0,0);
    if(!valid||clicks==0||candidate.size()!=clicks)return false;
    *targets=candidate;return true;
}
inline bool findSimpleEntrance(const QJsonObject& root,QString* target){
    if(!target)return false;
    target->clear();QStringList targets;
    if(!findClickEntrances(root,&targets)||targets.size()!=1)return false;
    *target=targets.first();return true;
}
}
