// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "profile.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

// ============================================================================
// ActionConfig
// ============================================================================

ActionConfig ActionConfig::fromJson(const QJsonObject &json)
{
    ActionConfig config;

    QString typeStr = json[QStringLiteral("type")].toString();
    if (typeStr == QStringLiteral("keyboard")) {
        config.type = Type::Keyboard;
    } else if (typeStr == QStringLiteral("keyboardRepeat")) {
        config.type = Type::KeyboardRepeat;
    } else if (typeStr == QStringLiteral("mouseScroll")) {
        config.type = Type::MouseScroll;
    } else if (typeStr == QStringLiteral("mouseMove")) {
        config.type = Type::MouseMove;
    } else if (typeStr == QStringLiteral("dbus")) {
        config.type = Type::DBusCall;
    } else if (typeStr == QStringLiteral("command")) {
        config.type = Type::Command;
    }

    config.keys = json[QStringLiteral("keys")].toString();

    // Parse modifiers
    QJsonArray modArray = json[QStringLiteral("modifiers")].toArray();
    for (const auto &mod : modArray) {
        QString modStr = mod.toString().toLower();
        if (modStr == QStringLiteral("ctrl") || modStr == QStringLiteral("control")) {
            config.modifiers |= Qt::ControlModifier;
        } else if (modStr == QStringLiteral("shift")) {
            config.modifiers |= Qt::ShiftModifier;
        } else if (modStr == QStringLiteral("alt")) {
            config.modifiers |= Qt::AltModifier;
        } else if (modStr == QStringLiteral("meta") || modStr == QStringLiteral("super")) {
            config.modifiers |= Qt::MetaModifier;
        }
    }

    config.rotationThreshold = json[QStringLiteral("rotationThreshold")].toInt(15);
    config.accelerationEnabled = json[QStringLiteral("accelerationEnabled")].toBool(true);

    // Mouse scroll
    config.delta = json[QStringLiteral("delta")].toInt(0);

    // D-Bus
    config.dbusService = json[QStringLiteral("dbusService")].toString();
    config.dbusPath = json[QStringLiteral("dbusPath")].toString();
    config.dbusInterface = json[QStringLiteral("dbusInterface")].toString();
    config.dbusMethod = json[QStringLiteral("dbusMethod")].toString();

    QJsonArray argsArray = json[QStringLiteral("dbusArgs")].toArray();
    for (const auto &arg : argsArray) {
        config.dbusArgs.append(arg.toVariant());
    }

    // Command
    config.command = json[QStringLiteral("command")].toString();

    return config;
}

QJsonObject ActionConfig::toJson() const
{
    QJsonObject json;

    QString typeStr;
    switch (type) {
        case Type::Keyboard: typeStr = QStringLiteral("keyboard"); break;
        case Type::KeyboardRepeat: typeStr = QStringLiteral("keyboardRepeat"); break;
        case Type::MouseScroll: typeStr = QStringLiteral("mouseScroll"); break;
        case Type::MouseMove: typeStr = QStringLiteral("mouseMove"); break;
        case Type::DBusCall: typeStr = QStringLiteral("dbus"); break;
        case Type::Command: typeStr = QStringLiteral("command"); break;
    }
    json[QStringLiteral("type")] = typeStr;

    json[QStringLiteral("keys")] = keys;

    // Modifiers
    QJsonArray modArray;
    if (modifiers & Qt::ControlModifier) modArray.append(QStringLiteral("ctrl"));
    if (modifiers & Qt::ShiftModifier) modArray.append(QStringLiteral("shift"));
    if (modifiers & Qt::AltModifier) modArray.append(QStringLiteral("alt"));
    if (modifiers & Qt::MetaModifier) modArray.append(QStringLiteral("meta"));
    json[QStringLiteral("modifiers")] = modArray;

    json[QStringLiteral("rotationThreshold")] = rotationThreshold;
    json[QStringLiteral("accelerationEnabled")] = accelerationEnabled;

    if (type == Type::MouseScroll) {
        json[QStringLiteral("delta")] = delta;
    }

    if (type == Type::DBusCall) {
        json[QStringLiteral("dbusService")] = dbusService;
        json[QStringLiteral("dbusPath")] = dbusPath;
        json[QStringLiteral("dbusInterface")] = dbusInterface;
        json[QStringLiteral("dbusMethod")] = dbusMethod;

        QJsonArray argsArray;
        for (const auto &arg : dbusArgs) {
            argsArray.append(QJsonValue::fromVariant(arg));
        }
        json[QStringLiteral("dbusArgs")] = argsArray;
    }

    if (type == Type::Command) {
        json[QStringLiteral("command")] = command;
    }

    return json;
}

// ============================================================================
// Function
// ============================================================================

Function Function::fromJson(const QJsonObject &json)
{
    Function func;

    func.id = json[QStringLiteral("id")].toString();
    func.label = json[QStringLiteral("label")].toString();
    func.iconName = json[QStringLiteral("iconName")].toString();

    QString typeStr = json[QStringLiteral("type")].toString();
    func.type = (typeStr == QStringLiteral("discrete")) ? Type::Discrete : Type::Continuous;

    if (json.contains(QStringLiteral("minValue"))) {
        func.minValue = json[QStringLiteral("minValue")].toDouble();
    }
    if (json.contains(QStringLiteral("maxValue"))) {
        func.maxValue = json[QStringLiteral("maxValue")].toDouble();
    }

    func.unit = json[QStringLiteral("unit")].toString();

    if (json.contains(QStringLiteral("clockwiseAction"))) {
        func.clockwiseAction = ActionConfig::fromJson(json[QStringLiteral("clockwiseAction")].toObject());
    }
    if (json.contains(QStringLiteral("counterClockwiseAction"))) {
        func.counterClockwiseAction = ActionConfig::fromJson(json[QStringLiteral("counterClockwiseAction")].toObject());
    }
    if (json.contains(QStringLiteral("clickAction"))) {
        func.clickAction = ActionConfig::fromJson(json[QStringLiteral("clickAction")].toObject());
    }

    return func;
}

QJsonObject Function::toJson() const
{
    QJsonObject json;

    json[QStringLiteral("id")] = id;
    json[QStringLiteral("label")] = label;
    json[QStringLiteral("iconName")] = iconName;
    json[QStringLiteral("type")] = (type == Type::Discrete) ? QStringLiteral("discrete") : QStringLiteral("continuous");

    if (minValue.has_value()) {
        json[QStringLiteral("minValue")] = minValue.value();
    }
    if (maxValue.has_value()) {
        json[QStringLiteral("maxValue")] = maxValue.value();
    }

    json[QStringLiteral("unit")] = unit;

    json[QStringLiteral("clockwiseAction")] = clockwiseAction.toJson();
    json[QStringLiteral("counterClockwiseAction")] = counterClockwiseAction.toJson();

    return json;
}

// ============================================================================
// FunctionGroup
// ============================================================================

FunctionGroup FunctionGroup::fromJson(const QJsonObject &json)
{
    FunctionGroup group;

    group.id = json[QStringLiteral("id")].toString();
    group.label = json[QStringLiteral("label")].toString();
    group.iconName = json[QStringLiteral("iconName")].toString();

    QJsonArray idsArray = json[QStringLiteral("functionIds")].toArray();
    for (const auto &idValue : idsArray) {
        group.functionIds.append(idValue.toString());
    }

    return group;
}

QJsonObject FunctionGroup::toJson() const
{
    QJsonObject json;

    json[QStringLiteral("id")] = id;
    json[QStringLiteral("label")] = label;
    json[QStringLiteral("iconName")] = iconName;

    QJsonArray idsArray;
    for (const auto &id : functionIds) {
        idsArray.append(id);
    }
    json[QStringLiteral("functionIds")] = idsArray;

    return json;
}

// ============================================================================
// Profile
// ============================================================================

void Profile::compileRegexes() const
{
    if (m_regexCompiled) {
        return;
    }

    if (!windowClassPattern.isEmpty()) {
        m_windowClassRegex = QRegularExpression(windowClassPattern, QRegularExpression::CaseInsensitiveOption);
    }
    if (!windowTitlePattern.isEmpty()) {
        m_windowTitleRegex = QRegularExpression(windowTitlePattern, QRegularExpression::CaseInsensitiveOption);
    }

    m_regexCompiled = true;
}

bool Profile::matches(const QString &windowClass,
                      const QString &windowTitle,
                      const QString &processName) const
{
    // Default profile matches everything
    if (isDefault) {
        return true;
    }

    // Check process names
    for (const QString &procName : processNames) {
        if (processName.contains(procName, Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Check window class pattern
    if (!windowClassPattern.isEmpty()) {
        compileRegexes();
        if (m_windowClassRegex.isValid() && m_windowClassRegex.match(windowClass).hasMatch()) {
            return true;
        }
    }

    // Check window title pattern
    if (!windowTitlePattern.isEmpty()) {
        compileRegexes();
        if (m_windowTitleRegex.isValid() && m_windowTitleRegex.match(windowTitle).hasMatch()) {
            return true;
        }
    }

    return false;
}

const Function* Profile::getFunction(const QString &functionId) const
{
    for (const auto &func : functions) {
        if (func.id == functionId) {
            return &func;
        }
    }
    return nullptr;
}

Function* Profile::getFunction(const QString &functionId)
{
    for (auto &func : functions) {
        if (func.id == functionId) {
            return &func;
        }
    }
    return nullptr;
}

QVector<Function> Profile::getMenuFunctions() const
{
    QVector<Function> menuFuncs;
    for (const auto &funcId : defaultMenuLayout) {
        const Function *func = getFunction(funcId);
        if (func) {
            menuFuncs.append(*func);
        }
    }
    return menuFuncs;
}

Profile Profile::fromJson(const QJsonObject &json)
{
    Profile profile;

    profile.id = json[QStringLiteral("id")].toString();
    profile.displayName = json[QStringLiteral("displayName")].toString();
    profile.iconPath = json[QStringLiteral("iconPath")].toString();
    profile.windowClassPattern = json[QStringLiteral("windowClassPattern")].toString();
    profile.windowTitlePattern = json[QStringLiteral("windowTitlePattern")].toString();
    profile.isDefault = json[QStringLiteral("isDefault")].toBool(false);

    QJsonArray procArray = json[QStringLiteral("processNames")].toArray();
    for (const auto &proc : procArray) {
        profile.processNames.append(proc.toString());
    }

    QJsonArray funcArray = json[QStringLiteral("functions")].toArray();
    for (const auto &funcValue : funcArray) {
        profile.functions.append(Function::fromJson(funcValue.toObject()));
    }

    QJsonArray groupArray = json[QStringLiteral("functionGroups")].toArray();
    for (const auto &groupValue : groupArray) {
        profile.groups.append(FunctionGroup::fromJson(groupValue.toObject()));
    }

    QJsonArray layoutArray = json[QStringLiteral("defaultMenuLayout")].toArray();
    for (const auto &idValue : layoutArray) {
        profile.defaultMenuLayout.append(idValue.toString());
    }

    profile.defaultFunctionId = json[QStringLiteral("defaultFunctionId")].toString();

    return profile;
}

Profile Profile::fromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open profile file:" << filePath;
        return Profile();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON in profile file:" << filePath;
        return Profile();
    }

    return fromJson(doc.object());
}

QJsonObject Profile::toJson() const
{
    QJsonObject json;

    json[QStringLiteral("id")] = id;
    json[QStringLiteral("displayName")] = displayName;
    json[QStringLiteral("iconPath")] = iconPath;
    json[QStringLiteral("windowClassPattern")] = windowClassPattern;
    json[QStringLiteral("windowTitlePattern")] = windowTitlePattern;
    json[QStringLiteral("isDefault")] = isDefault;

    QJsonArray procArray;
    for (const auto &proc : processNames) {
        procArray.append(proc);
    }
    json[QStringLiteral("processNames")] = procArray;

    QJsonArray funcArray;
    for (const auto &func : functions) {
        funcArray.append(func.toJson());
    }
    json[QStringLiteral("functions")] = funcArray;

    QJsonArray groupArray;
    for (const auto &group : groups) {
        groupArray.append(group.toJson());
    }
    json[QStringLiteral("functionGroups")] = groupArray;

    QJsonArray layoutArray;
    for (const auto &id : defaultMenuLayout) {
        layoutArray.append(id);
    }
    json[QStringLiteral("defaultMenuLayout")] = layoutArray;

    json[QStringLiteral("defaultFunctionId")] = defaultFunctionId;

    return json;
}
