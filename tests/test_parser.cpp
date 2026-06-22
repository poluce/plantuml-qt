#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "../src/parser/PumlParser.h"
#include "../src/ast/DiagramAst.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = "class_diagram.puml";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName("../class_diagram.puml");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "Cannot open class_diagram.puml!";
            return 1;
        }
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    qDebug() << "Loaded class_diagram.puml successfully. Length:" << content.length();

    PumlParser parser(content);
    ParseResult result = parser.parse();

    if (result.ast) {
        qDebug() << "Is Class Diagram:" << (!result.ast->isSequence());
    } else {
        qDebug() << "AST is null!";
    }
    qDebug() << "Error count:" << result.errors.size();

    for (const auto &err : result.errors) {
        qWarning() << "Line" << err.location.line << ":" << err.message;
    }

    return result.errors.isEmpty() ? 0 : 1;
}
