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
// Slide order is from presentation.xml and its relationships, NOT slide file names.
QStringList slideEntries(const QString& source, QString* error);
Result patchXml(const QByteArray& xml, const Options& options, QByteArray* output);
Result createCopy(const QString& source, const QString& destination, int slide1, const Options& options);
struct Shape { unsigned id = 0; QString name; };
struct AnimationOptions { unsigned shapeId = 0; bool fade = false; int durationMs = 500; int delayMs = 0; };
QList<Shape> slideShapes(const QString& source, int slide1, QString* error);
Result patchAnimationXml(const QByteArray& xml, const AnimationOptions& options, QByteArray* output);
Result createAnimationCopy(const QString& source, const QString& destination, int slide1, const AnimationOptions& options);
Result patchAnimationSequenceXml(const QByteArray& xml, const QList<AnimationOptions>& effects, QByteArray* output);
Result createAnimationSequenceCopy(const QString& source, const QString& destination, int slide1, const QList<AnimationOptions>& effects);
// Modify the actual referenced master part, not individual slides.
QStringList masterEntries(const QString& source, QString* error);
Result patchMasterBackground(const QByteArray& xml, const QString& rgb, QByteArray* output);
Result createMasterBackgroundCopy(const QString& source, const QString& destination, const QString& entry, const QString& rgb);
}
