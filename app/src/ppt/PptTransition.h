// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
namespace ppttransition {
enum class Effect { None, Fade, PushLeft, WipeLeft, Dissolve };
struct Options { Effect effect = Effect::Fade; int speed = 1; bool onClick = true; int autoAdvanceMs = 0; };
struct Result { bool ok = false; QString error; };
// Slide order is from presentation.xml and its relationships, NOT slide file names.
QStringList slideEntries(const QString& source, QString* error);
Result patchXml(const QByteArray& xml, const Options& options, QByteArray* output);
Result createCopy(const QString& source, const QString& destination, int slide1, const Options& options);
}
