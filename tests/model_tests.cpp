/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2026 RF StateSide LLC
 *
 * This file is part of RayNmeaSimulator.
 *
 * RayNmeaSimulator is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Affero General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any
 * later version.
 *
 * RayNmeaSimulator is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General
 * Public License along with RayNmeaSimulator.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <QtTest>
#include <QAbstractItemModelTester>
#include "pgntablemodel.h"
class ModelTests : public QObject {
 Q_OBJECT
private slots:
 void receiveWithoutFocus() {
  PgnTableModel m;
  m.addPgn(1,"Position","129025",true,{{"Latitude","42.5","deg","double"}},{});
  m.addPgn(2,"Other","129025",true,{{"Latitude","10","deg","double"}},{});
  m.setIdxHasFocus(-1);
  QSignalSpy updates(&m,&PgnTableModel::parameterChanged);
  m.changeParams(1,{"Latitude","42.5001"});
  QCOMPARE(m.getPgnParams(1)[0][1],QString("42.5001"));
  QCOMPARE(updates.count(),1);
  QCOMPARE(m.rowCount(),0); // No Property Editor selection.
  QCOMPARE(m.getPgnParams(2)[0][1],QString("10"));
  m.setIdxHasFocus(2);
  QCOMPARE(m.data(m.index(0,1)).toDouble(),10.0);
  m.changeParams(1,{"Latitude","42.5001"});
  QCOMPARE(updates.count(),1); // No redundant notifications.
  m.removePgn(1);
  m.changeParams(1,{"Latitude","43"});
  QCOMPARE(updates.count(),1); // No notification for a removed instrument.
 }
 void editorPersists() {
  PgnTableModel m;
  m.addPgn(7, "Tank", "127505", true, {{"Level","25","%","double"}}, {{0,100}});
  QSignalSpy changes(&m, &PgnTableModel::parameterChanged);
  QVERIFY(m.setData(m.index(0,1),40));
  QCOMPARE(m.getPgnParams(7)[0][1],QString("40"));
  QCOMPARE(m.data(m.index(0,1)).toString(),QString("40"));
  QCOMPARE(changes.count(),1);
 }
 void widgetUpdatesSelection() {
  PgnTableModel m;
  m.addPgn(7,"Tank","127505",true,{{"Level","0","%","double"}},{{0,100}});
  QSignalSpy changed(&m,&QAbstractItemModel::dataChanged);
  m.changeParam(7,"Level","25");
  QCOMPARE(m.data(m.index(0,1)).toString(),QString("25"));
  QCOMPARE(changed.count(),1);
 }
 void selectionAndRemoval() {
  PgnTableModel m;
  QAbstractItemModelTester tester(&m,QAbstractItemModelTester::FailureReportingMode::QtTest);
  m.addPgn(7,"A","127505",true,{{"Level","25","%","double"}},{{0,100}});
  m.addPgn(9,"B","127505",true,{{"Instance","1","","int"},{"Level","0","%","double"}},{{0,255},{0,100}});
  m.setCurrentPgn(7);
  QCOMPARE(m.rowCount(),1);
  m.removePgn(9);
  QCOMPARE(m.rowCount(),1);
  m.removePgn(7);
  QCOMPARE(m.rowCount(),0);
  m.clearModel();
  QVERIFY(!m.setData(QModelIndex(),1));
 }
 void typedValuesAndIndependentInstances() {
  PgnTableModel m;
  const QList<QList<QString>> p={{"Instance","0","","unsigned char"},{"FluidType","0","enum","enum"},{"Level","0","%","double"},{"Enabled","0","enum","enum"}};
  m.addPgn(1,"A","127505",true,p,{});
  m.addPgn(2,"B","127505",true,p,{});
  m.setCurrentPgn(1);
  for(int i=0;i<4;++i) QVERIFY(m.setData(m.index(i,1),QString::number(i+1)));
  m.setPgnEnabled(1,false);
  for(int i=0;i<4;++i) QCOMPARE(m.getPgnParams(1)[i][1],QString::number(i+1));
  QCOMPARE(m.getPgnParams(2)[2][1],QString("0"));
  QVERIFY(!m.isPgnEnabled(1));
  QVERIFY(m.isPgnEnabled(2));
  m.updatePgnParameter(2,"Level",55,false);
  QCOMPARE(m.data(m.index(2,1)).toString(),QString("3"));
  m.setCurrentPgn(2);
  QCOMPARE(m.data(m.index(2,1)).toString(),QString("55"));
 }
};
QTEST_MAIN(ModelTests)
#include "model_tests.moc"
