// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
namespace slideshow {
// Only one entire-shape, on-click, instant visibility entrance is supported.
// Unknown, timed, multi-effect or interactive animations keep static rendering.
inline bool findSimpleEntrance(const QJsonObject& root,QString* target){
    if(!target)return false;
    target->clear();
    int clicks=0,leaves=0,nodes=0;bool valid=true;
    auto visit=[&](auto&& self,const QJsonObject& node,int depth,bool inClick)->void{
        if(!valid||depth>32||++nodes>128){valid=false;return;}
        const auto type=node.value("nodeType").toString(),kind=node.value("nodeName").toString();
        if(type=="InteractiveSequence"||type=="WithPrevious"||type=="AfterPrevious"||
           node.contains("repeatCount")||node.contains("autoreverse")){valid=false;return;}
        if(type=="OnClick"){++clicks;inClick=true;}
        const auto children=node.value("children");
        if(kind=="Par"||kind=="Seq"){
            if(!children.isArray()||children.toArray().isEmpty()){valid=false;return;}
            for(const auto& child:children.toArray()){if(!child.isObject()){valid=false;return;}self(self,child.toObject(),depth+1,inClick);}
            return;
        }
        ++leaves;
        const auto name=node.value("attributeName").toString(),value=node.value("to").toString().toLower();
        const auto sub=node.value("subItem").toString();
        if(!inClick||kind!="Set"||name!="Visibility"||(value!="visible"&&value!="true")||
           (!sub.isEmpty()&&sub!="AsWhole")){valid=false;return;}
        *target=node.value("targetElement").toString();
        if(target->isEmpty())valid=false;
    };
    visit(visit,root,0,false);
    return valid&&clicks==1&&leaves==1&&!target->isEmpty();
}
}
