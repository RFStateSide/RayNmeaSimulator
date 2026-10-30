#ifndef GENERATORTRANSLOWERUNIT_H
#define GENERATORTRANSLOWERUNIT_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GeneratorTransLowerUnit;
}

class GeneratorTransLowerUnit : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GeneratorTransLowerUnit(int id, QString label, QWidget *parent = nullptr);
    ~GeneratorTransLowerUnit();

private:
    Ui::GeneratorTransLowerUnit *ui;
};

#endif // GENERATORTRANSLOWERUNIT_H
