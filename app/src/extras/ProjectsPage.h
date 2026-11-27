// SPDX-License-Identifier: MPL-2.0
// Read-only directory of the author's three project websites.
#pragma once
#include <QWidget>

class ProjectsPage : public QWidget {
    Q_OBJECT
public:
    explicit ProjectsPage(QWidget* parent = nullptr);
    void flush(); // compatibility with workspace lifecycle; no writes
};
