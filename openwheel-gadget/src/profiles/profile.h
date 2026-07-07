// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QVariantList>
#include <QRegularExpression>
#include <optional>

/**
 * Configuration for a single action triggered by dial rotation
 */
struct ActionConfig {
    enum class Type {
        Keyboard,           // Single keystroke
        KeyboardRepeat,     // Repeated keystrokes based on rotation
        MouseScroll,        // Simulate scroll wheel
        MouseMove,          // Simulate mouse movement
        DBusCall,           // Call D-Bus method
        Command             // Execute shell command
    };

    Type type = Type::Keyboard;
    QString keys;                      // e.g., "]" or "Ctrl+Z"
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    int rotationThreshold = 15;        // Degrees per action trigger
    bool accelerationEnabled = true;

    // For mouse scroll
    int delta = 0;
    Qt::Orientation orientation = Qt::Vertical;

    // For D-Bus calls
    QString dbusService;
    QString dbusPath;
    QString dbusInterface;
    QString dbusMethod;
    QVariantList dbusArgs;

    // For commands
    QString command;

    // Sticky modifier: hold modifier keys across consecutive rotations (e.g.
    // Alt+Tab window switcher — Alt stays held while the user keeps rotating).
    // The modifiers are released when the dial button is released.
    bool sticky = false;

    static ActionConfig fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
};

/**
 * A function that can be assigned to the dial
 */
struct Function {
    QString id;
    QString label;
    QString iconName;                  // Freedesktop icon name or path

    enum class Type { Continuous, Discrete };
    Type type = Type::Continuous;

    std::optional<qreal> minValue;
    std::optional<qreal> maxValue;
    QString unit;                      // e.g., "px", "%"

    // When true the overlay HUD is hidden while this function is active.
    // Automatically true for mouseScroll/zoom actions; user can override.
    bool suppressOverlay = false;

    ActionConfig clockwiseAction;
    ActionConfig counterClockwiseAction;
    ActionConfig clickAction;          // Optional click behavior

    static Function fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
};

/**
 * A group of related functions
 */
struct FunctionGroup {
    QString id;
    QString label;
    QString iconName;
    QStringList functionIds;

    static FunctionGroup fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
};

/**
 * A complete profile for an application or context
 */
class Profile {
public:
    QString id;
    QString displayName;
    QString iconPath;
    QStringList processNames;          // e.g., ["photoshop", "Photoshop.exe"]
    QString windowClassPattern;        // Regex pattern
    QString windowTitlePattern;        // Regex pattern
    bool isDefault = false;            // Is this the default/fallback profile?
    bool enabled = true;               // false = hidden from picker (but editable in settings)

    QVector<Function> functions;
    QVector<FunctionGroup> groups;
    QStringList defaultMenuLayout;     // Function IDs for main menu
    QString defaultFunctionId;         // For single-function mode

    /**
     * Check if this profile matches the given window
     */
    bool matches(const QString &windowClass,
                 const QString &windowTitle,
                 const QString &processName) const;

    /**
     * Get a function by ID
     */
    const Function* getFunction(const QString &functionId) const;
    Function* getFunction(const QString &functionId);

    /**
     * Get all functions for the default menu
     */
    QVector<Function> getMenuFunctions() const;

    static Profile fromJson(const QJsonObject &json);
    static Profile fromFile(const QString &filePath);
    QJsonObject toJson() const;
    bool saveToFile(const QString &filePath) const;

private:
    mutable QRegularExpression m_windowClassRegex;
    mutable QRegularExpression m_windowTitleRegex;
    mutable bool m_regexCompiled = false;

    void compileRegexes() const;
};
