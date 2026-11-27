// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <QWidget>

// Three curated external websites; no project-management storage or mutations.
class ProjectsPage : public QWidget {
    Q_OBJECT
public:
    explicit ProjectsPage(QWidget* parent=nullptr);
};
