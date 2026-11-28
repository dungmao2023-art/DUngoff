// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
namespace ppttransition {
enum class Effect { None, Fade, PushLeft, WipeLeft, Dissolve };
struct Options { Effect effect = Effect::Fade; int speed = 1; bool onClick = true; int autoAdvanceMs = 0; };
struct Result { bool ok = false; QString error; };
// Full content identity; empty means unreadable, never a valid fingerprint.
QByteArray sourceFingerprint(const QString& source);
// Slide order is from presentation.xml and its relationships, NOT slide file names.
QStringList slideEntries(const QString& source, QString* error);
Result patchXml(const QByteArray& xml, const Options& options, QByteArray* output);
Result createCopy(const QString& source, const QString& destination, int slide1, const Options& options, const QByteArray& expectedSource = {});
// Inclusive 1-based slide range, resolved using presentation relationships.
Result createRangeCopy(const QString& source,const QString& destination,int first,int last,const Options& options, const QByteArray& expectedSource = {});
struct Shape { unsigned id = 0; QString name; };
enum class AnimationTrigger { Click, WithPrevious, AfterPrevious };
struct AnimationOptions { unsigned shapeId = 0; bool fade = false; int durationMs = 500; int delayMs = 0; AnimationTrigger trigger=AnimationTrigger::Click; bool exit = false; };
struct AnimationTiming { int click=0, startMs=0, endMs=0; };
// Deterministic per-click timeline. The first row must be click; no auto-start on slide entry yet.
Result planAnimation(const QList<AnimationOptions>& effects,QList<AnimationTiming>* timing);
// Student-friendly presets: one click per object, one click for a cascade, or all together.
enum class AnimationPreset { AppearClickEach, ClickEach, Cascade, Together };
inline Result makeAnimationPreset(const QList<Shape>& shapes, AnimationPreset preset,
                                  QList<AnimationOptions>* output) {
    if(!output || shapes.isEmpty() || shapes.size()>100)
        return {false,QStringLiteral("Chọn từ 1 đến 100 đối tượng; không cắt bớt danh sách.")};
    if(preset!=AnimationPreset::AppearClickEach && preset!=AnimationPreset::ClickEach &&
       preset!=AnimationPreset::Cascade && preset!=AnimationPreset::Together)
        return {false,QStringLiteral("Kiểu hoạt ảnh không hợp lệ.")};
    QList<AnimationOptions> candidate;
    for(const auto& shape:shapes){
        AnimationOptions effect;
        effect.shapeId=shape.id;
        effect.fade=preset!=AnimationPreset::AppearClickEach;
        if(!effect.fade)effect.durationMs=100;
        if(!candidate.isEmpty()){
            if(preset==AnimationPreset::Cascade)effect.trigger=AnimationTrigger::AfterPrevious;
            else if(preset==AnimationPreset::Together)effect.trigger=AnimationTrigger::WithPrevious;
        }
        candidate.append(effect);
    }
    QList<AnimationTiming> timing;
    const auto valid=planAnimation(candidate,&timing);
    if(!valid.ok)return valid;
    *output=candidate;
    return {true,{}};
}
QList<Shape> slideShapes(const QString& source, int slide1, QString* error);
// Remove only a verified direct slide timing element, never transition or shape content.
Result clearAnimationXml(const QByteArray& xml,QByteArray* output);
Result createClearAnimationCopy(const QString& source,const QString& destination,int slide1, const QByteArray& expectedSource = {});
// Inclusive range, already static slides left unchanged; malformed XML aborts the entire copy.
Result createClearAnimationRangeCopy(const QString& source,const QString& destination,int first,int last,const QByteArray& expectedSource = {});
Result patchAnimationXml(const QByteArray& xml, const AnimationOptions& options, QByteArray* output);
Result createAnimationCopy(const QString& source, const QString& destination, int slide1, const AnimationOptions& options, const QByteArray& expectedSource = {});
Result patchAnimationSequenceXml(const QByteArray& xml, const QList<AnimationOptions>& effects, QByteArray* output);
Result createAnimationSequenceCopy(const QString& source, const QString& destination, int slide1, const QList<AnimationOptions>& effects, const QByteArray& expectedSource = {});
// Modify the actual referenced master part, not individual slides.
QStringList masterEntries(const QString& source, QString* error);
Result patchMasterBackground(const QByteArray& xml, const QString& rgb, QByteArray* output);
Result createMasterBackgroundCopy(const QString& source, const QString& destination, const QString& entry, const QString& rgb, const QByteArray& expectedSource = {});
}
