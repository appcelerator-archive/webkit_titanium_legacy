
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qgraphicswebview qwebhistoryinterface qwebview qwebhistory qwebinspector
greaterThan(QT_MINOR_VERSION, 4): SUBDIRS += benchmarks/painting/tst_painting.pro benchmarks/loading/tst_loading.pro
