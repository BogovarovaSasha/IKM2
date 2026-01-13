#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnAddBook_clicked();
    void on_btnEdit_clicked();
    void on_btnDelete_clicked();
    void on_btnAddQuote_clicked();
    void on_btnShowQuotes_clicked();
    void on_btnAddToShelf_clicked();
    void on_listBooks_currentRowChanged(int row);

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    int currentBookId;

    bool connectToDatabase();
    void loadBooksFromDB();
    void loadShelvesList();
    void loadBookDetails(int bookId);
    void updateBookCount();
    void addToShelf(int bookId, int shelfId);
};

#endif // MAINWINDOW_H
