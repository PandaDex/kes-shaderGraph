#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

namespace sne {

class SlangHighlighter : public QSyntaxHighlighter {
public:
    explicit SlangHighlighter(QTextDocument* doc);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression re;
        QTextCharFormat fmt;
    };
    QVector<Rule> m_rules;
    QTextCharFormat m_commentFmt;
};

} // namespace sne
