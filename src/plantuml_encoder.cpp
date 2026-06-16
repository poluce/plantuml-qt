#include "plantuml_encoder.h"
#include <QByteArray>

PlantUmlEncoder::PlantUmlEncoder(QObject *parent)
    : QObject(parent)
{
}

QString PlantUmlEncoder::encode(const QString &text) const
{
    if (text.isEmpty()) {
        return QString();
    }

    // 1. 转为 UTF-8 字节流
    QByteArray utf8 = text.toUtf8();
    
    // 2. 使用 qCompress 压缩。qCompress 产生的数据带有 4 字节的原始数据长度头部，需要移除以获得标准 deflate 数据
    QByteArray compressed = qCompress(utf8);
    if (compressed.size() <= 4) {
        return QString();
    }
    
    QByteArray deflateData = compressed.mid(4);
    
    // 3. 按照 PlantUML 官方规范进行 3-to-4 变体 Base64 编码
    static const char codes[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
    QString result;
    int len = deflateData.size();
    
    for (int i = 0; i < len; i += 3) {
        int b1 = deflateData[i] & 0xFF;
        int b2 = (i + 1 < len) ? (deflateData[i + 1] & 0xFF) : 0;
        int b3 = (i + 2 < len) ? (deflateData[i + 2] & 0xFF) : 0;
        
        int c1 = b1 >> 2;
        int c2 = ((b1 & 0x3) << 4) | (b2 >> 4);
        int c3 = ((b2 & 0xF) << 2) | (b3 >> 6);
        int c4 = b3 & 0x3F;
        
        result.append(codes[c1 & 0x3F]);
        result.append(codes[c2 & 0x3F]);
        result.append(codes[c3 & 0x3F]);
        result.append(codes[c4 & 0x3F]);
    }
    
    return result;
}
