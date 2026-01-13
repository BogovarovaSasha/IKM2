#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QListWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , currentBookId(-1)
{
    ui->setupUi(this);
    connect(ui->listBooks, &QListWidget::currentRowChanged,
            this, &MainWindow::on_listBooks_currentRowChanged);

    if (!connectToDatabase()) {
        return;
    }
    QSqlQuery testQuery;
    if (testQuery.exec("SELECT COUNT(*) as total, "
                      "SUM(CASE WHEN status='planned' THEN 1 ELSE 0 END) as planned, "
                      "SUM(CASE WHEN status='reading' THEN 1 ELSE 0 END) as reading, "
                      "SUM(CASE WHEN status='finished' THEN 1 ELSE 0 END) as finished "
                      "FROM books")) {
    } else {
        qDebug() << "Ошибка выполнения запроса статистики:" << testQuery.lastError().text();
    }

    loadBooksFromDB();
    loadShelvesList();
}
MainWindow::~MainWindow()
{
    if (db.isOpen()) {
        db.close();
    }
    delete ui;
}
// ============ ПОДКЛЮЧЕНИЕ К БАЗЕ ============
bool MainWindow::connectToDatabase()
{

    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setDatabaseName("bo1");
    db.setUserName("postgres");
    db.setPassword("12345");
    db.setPort(5432);

    if (!db.open()) {
        QString error = db.lastError().text();
        qDebug() << "Ошибка подключения:" << error;
}
    qDebug() << "Успешное подключение к PostgreSQL!";
    return true;
}
// ============ ЗАГРУЗКА КНИГ ============
void MainWindow::loadBooksFromDB()
{
    ui->listBooks->clear();

    QSqlQuery query;
    bool success = query.exec("SELECT id, title, author, status, rating FROM books ORDER BY title");

    qDebug() << "Запрос выполнен?" << success;
    if (!success) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        QMessageBox::warning(this, "Ошибка",
            "Не удалось загрузить книги:\n" + query.lastError().text());
        return;
    }

    int count = 0;
    while (query.next()) {
        count++;
        int id = query.value(0).toInt();
        QString title = query.value(1).toString();
        QString author = query.value(2).toString();
        QString status = query.value(3).toString();
        int rating = query.value(4).toInt();

        QString bookInfo = title + " - " + author;

        QString statusIcon;
        if (status == "planned") statusIcon = "⏳";
        else if (status == "reading") statusIcon = "📖";
        else if (status == "finished") statusIcon = "✅";

        bookInfo += " " + statusIcon;

        if (rating > 0) {
            QString stars;
            for (int i = 0; i < rating; i++) stars += "★";
            for (int i = rating; i < 5; i++) stars += "☆";
            bookInfo += " " + stars;
        }

        QListWidgetItem *item = new QListWidgetItem(bookInfo);
        item->setData(Qt::UserRole, id);

        if (status == "reading") {
            item->setBackground(QColor(232, 245, 233));
            item->setForeground(QColor(27, 94, 32));
        }
        else if (status == "finished") {
            item->setBackground(QColor(243, 229, 245));
            item->setForeground(QColor(74, 20, 140));
        }
        else if (status == "planned") {
            item->setBackground(QColor(232, 244, 252));
            item->setForeground(QColor(13, 71, 161));
        }
        ui->listBooks->addItem(item);
    }

    qDebug() << "Загружено книг:" << count;

    for (int i = 0; i < ui->listBooks->count(); i++) {
        QListWidgetItem *item = ui->listBooks->item(i);
        int bookId = item->data(Qt::UserRole).toInt();

        QSqlQuery shelfQuery;
        shelfQuery.prepare(
            "SELECT s.name FROM shelves s "
            "JOIN book_shelves bs ON s.id = bs.shelf_id "
            "WHERE bs.book_id = ?"
        );
        shelfQuery.addBindValue(bookId);

        if (shelfQuery.exec()) {
            QStringList shelves;
            while (shelfQuery.next()) {
                shelves << shelfQuery.value(0).toString();
            }

            if (!shelves.isEmpty()) {
                QString currentText = item->text();
                currentText += " [" + shelves.join(", ") + "]";
                item->setText(currentText);
            }
        }
    }
    updateBookCount();
}
// ============ ОБНОВЛЕНИЕ СЧЁТЧИКА ============
void MainWindow::updateBookCount()
{
    QSqlQuery query;

    query.exec("SELECT COUNT(*) FROM books");
    query.next();
    int total = query.value(0).toInt();

    query.exec("SELECT status, COUNT(*) FROM books GROUP BY status");
    QString statusInfo;
    while (query.next()) {
        QString status = query.value(0).toString();
        int count = query.value(1).toInt();

        if (status == "planned") statusInfo += " ⏳:" + QString::number(count);
        else if (status == "reading") statusInfo += " 📖:" + QString::number(count);
        else if (status == "finished") statusInfo += " ✅:" + QString::number(count);
    }

    ui->labelCount->setText("Всего: " + QString::number(total) + " книг" + statusInfo);
}

// ============ ЗАГРУЗКА ДЕТАЛЕЙ КНИГИ ============
void MainWindow::loadBookDetails(int bookId)
{
    if (bookId == -1) {
        ui->labelDetails->setText("Выберите книгу для просмотра деталей");
        return;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT title, author, genre, year, pages, status, "
        "rating, notes, start_date, finish_date "
        "FROM books WHERE id = ?"
    );
    query.addBindValue(bookId);

    if (!query.exec() || !query.next()) {
        ui->labelDetails->setText("Не удалось загрузить информацию о книге");
        return;
    }

    QString details = "<b>📚 " + query.value(0).toString() + "</b><br>";
    details += "✍️ Автор: " + query.value(1).toString() + "<br>";

    if (!query.value(2).isNull()) {
        details += "🏷️ Жанр: " + query.value(2).toString() + "<br>";
    }

    if (query.value(3).toInt() > 0) {
        details += "📅 Год: " + query.value(3).toString() + "<br>";
    }

    if (query.value(4).toInt() > 0) {
        details += "📄 Страниц: " + query.value(4).toString() + "<br>";
    }

    QString status = query.value(5).toString();
    QString statusText;
    if (status == "planned") statusText = "⏳ Хочу прочитать";
    else if (status == "reading") statusText = "📖 Читаю сейчас";
    else if (status == "finished") statusText = "✅ Прочитано";
    details += "📖 Статус: " + statusText + "<br>";

    if (!query.value(6).isNull()) {
        int rating = query.value(6).toInt();
        QString stars;
        for (int i = 0; i < rating; i++) stars += "★";
        details += "⭐ Оценка: " + stars + " (" + QString::number(rating) + "/5)<br>";
    }

    if (!query.value(7).isNull() && !query.value(7).toString().isEmpty()) {
        details += "📝 Заметки: " + query.value(7).toString() + "<br>";
    }

    ui->labelDetails->setText(details);
    currentBookId = bookId;
}

// ============ ЗАГРУЗКА СПИСКА ПОЛОК ============
void MainWindow::loadShelvesList()
{
    ui->listShelves->clear();

    QSqlQuery query("SELECT id, name, description FROM shelves ORDER BY name");
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        QString desc = query.value(2).toString();

        QString itemText = "📚 " + name;
        if (!desc.isEmpty()) {
            itemText += "\n   " + desc;
        }

        QListWidgetItem *item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, id);
        ui->listShelves->addItem(item);
    }
}

// ============ ДОБАВЛЕНИЕ КНИГИ (ПОЛНАЯ ВЕРСИЯ) ============
void MainWindow::on_btnAddBook_clicked()
{
    // Простой диалог с расширенными полями
    QDialog dialog(this);
    dialog.setWindowTitle("➕ Добавить книгу");
    dialog.setFixedSize(350, 350);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *labelTitle = new QLabel("Название книги:*");
    QLineEdit *editTitle = new QLineEdit();
    layout->addWidget(labelTitle);
    layout->addWidget(editTitle);

    QLabel *labelAuthor = new QLabel("Автор:*");
    QLineEdit *editAuthor = new QLineEdit();
    layout->addWidget(labelAuthor);
    layout->addWidget(editAuthor);

    QLabel *labelGenre = new QLabel("Жанр (необязательно):");
    QLineEdit *editGenre = new QLineEdit();
    layout->addWidget(labelGenre);
    layout->addWidget(editGenre);

    QLabel *labelStatus = new QLabel("Статус:");
    QComboBox *comboStatus = new QComboBox();
    comboStatus->addItem("⏳ Хочу прочитать", "planned");
    comboStatus->addItem("📖 Читаю сейчас", "reading");
    comboStatus->addItem("✅ Прочитано", "finished");
    layout->addWidget(labelStatus);
    layout->addWidget(comboStatus);

    QLabel *labelRating = new QLabel("Оценка (1-5):");
    QSpinBox *spinRating = new QSpinBox();
    spinRating->setRange(0, 5);
    spinRating->setSpecialValueText("не оценено");
    spinRating->setValue(0);
    layout->addWidget(labelRating);
    layout->addWidget(spinRating);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *btnOk = new QPushButton("💾 Сохранить");
    QPushButton *btnCancel = new QPushButton("❌ Отмена");

    btnOk->setStyleSheet("background-color: #28a745; color: white; padding: 8px; border-radius: 5px;");
    btnCancel->setStyleSheet("background-color: #dc3545; color: white; padding: 8px; border-radius: 5px;");

    buttonLayout->addStretch();
    buttonLayout->addWidget(btnOk);
    buttonLayout->addWidget(btnCancel);
    layout->addLayout(buttonLayout);

    // Обработка
    bool dialogAccepted = false;

    connect(btnOk, &QPushButton::clicked, [&]() {
        if (editTitle->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "Ошибка", "Введите название книги!");
            return;
        }
        if (editAuthor->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "Ошибка", "Введите автора!");
            return;
        }
        dialogAccepted = true;
        dialog.accept();
    });

    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted && dialogAccepted) {
        QSqlQuery query;
        query.prepare(
            "INSERT INTO books (title, author, genre, status, rating) "
            "VALUES (?, ?, ?, ?, ?)"
        );

        query.addBindValue(editTitle->text().trimmed());
        query.addBindValue(editAuthor->text().trimmed());

        QString genre = editGenre->text().trimmed();
        if (genre.isEmpty()) {
            query.addBindValue(QVariant());
        } else {
            query.addBindValue(genre);
        }

        // Статус и рейтинг
        query.addBindValue(comboStatus->currentData().toString());

        int rating = spinRating->value();
        if (rating > 0) {
            query.addBindValue(rating);
        } else {
            query.addBindValue(QVariant());
        }

        if (query.exec()) {
            // Автоматически добавляем на полку
            QString status = comboStatus->currentData().toString();
            int newBookId = query.lastInsertId().toInt();

            QSqlQuery shelfQuery;
            if (status == "planned") {
                shelfQuery.prepare("SELECT id FROM shelves WHERE name = 'Хочу прочитать'");
            } else if (status == "reading") {
                shelfQuery.prepare("SELECT id FROM shelves WHERE name = 'Читаю сейчас'");
            } else if (status == "finished") {
                shelfQuery.prepare("SELECT id FROM shelves WHERE name = 'Прочитано'");
            }

            if (shelfQuery.exec() && shelfQuery.next()) {
                int shelfId = shelfQuery.value(0).toInt();

                QSqlQuery linkQuery;
                linkQuery.prepare("INSERT INTO book_shelves (book_id, shelf_id) VALUES (?, ?)");
                linkQuery.addBindValue(newBookId);
                linkQuery.addBindValue(shelfId);
                linkQuery.exec();
            }

            // Обновляем интерфейс
            loadBooksFromDB();
            QMessageBox::information(this, "Успех", "Книга добавлена! ✨");
        } else {
            QMessageBox::warning(this, "Ошибка",
                "Не удалось добавить книгу:\n" + query.lastError().text());
        }
    }
}
// ============ УДАЛЕНИЕ КНИГИ ============
void MainWindow::on_btnDelete_clicked()
{
    int row = ui->listBooks->currentRow();
    if (row == -1) {
        QMessageBox::warning(this, "Внимание", "Выберите книгу для удаления!");
        return;
    }

    QListWidgetItem *item = ui->listBooks->item(row);
    int bookId = item->data(Qt::UserRole).toInt();
    QString bookTitle = item->text().split(" - ")[0];

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Подтверждение",
        "Удалить книгу из базы данных?\n\"" + bookTitle + "\"?\n\n"
        "Это также удалит все цитаты и связи с полками.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM books WHERE id = ?");
        query.addBindValue(bookId);

        if (query.exec()) {
            loadBooksFromDB();
            ui->labelDetails->setText("Выберите книгу для просмотра деталей");
            QMessageBox::information(this, "Успех", "Книга удалена!");
        } else {
            QMessageBox::warning(this, "Ошибка",
                "Не удалось удалить книгу:\n" + query.lastError().text());
        }
    }
}

// ============ ВЫБОР КНИГИ В СПИСКЕ ============
void MainWindow::on_listBooks_currentRowChanged(int row)
{
    if (row == -1) {
        loadBookDetails(-1);
        return;
    }

    QListWidgetItem *item = ui->listBooks->item(row);
    int bookId = item->data(Qt::UserRole).toInt();
    loadBookDetails(bookId);
}

// ============ ДОБАВЛЕНИЕ ЦИТАТЫ ============
void MainWindow::on_btnAddQuote_clicked()
{
    if (currentBookId == -1) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите книгу!");
        return;
    }

    bool ok;
    QString quote = QInputDialog::getText(this, "Новая цитата",
        "Текст цитаты:", QLineEdit::Normal, "", &ok);
    if (!ok || quote.isEmpty()) return;

    int page = QInputDialog::getInt(this, "Новая цитата",
        "Номер страницы (0 если не указан):", 0, 0, 10000, 1, &ok);
    if (!ok) return;

    QSqlQuery query;
    query.prepare("INSERT INTO quotes (book_id, quote_text, page) VALUES (?, ?, ?)");
    query.addBindValue(currentBookId);
    query.addBindValue(quote);
    query.addBindValue(page);

    if (query.exec()) {
        QMessageBox::information(this, "Успех", "Цитата добавлена!");
    } else {
        QMessageBox::warning(this, "Ошибка",
            "Не удалось добавить цитату:\n" + query.lastError().text());
    }
}

// ============ ПОКАЗАТЬ ЦИТАТЫ ============
void MainWindow::on_btnShowQuotes_clicked()
{
    if (currentBookId == -1) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите книгу!");
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT quote_text, page FROM quotes WHERE book_id = ? ORDER BY page");
    query.addBindValue(currentBookId);

    if (!query.exec()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить цитаты");
        return;
    }

    QString quotesText = "<b>📝 Цитаты из книги:</b><br><br>";
    bool hasQuotes = false;

    while (query.next()) {
        hasQuotes = true;
        QString quote = query.value(0).toString();
        int page = query.value(1).toInt();

        quotesText += "❝" + quote + "❞";
        if (page > 0) {
            quotesText += " (стр. " + QString::number(page) + ")";
        }
        quotesText += "<br><br>";
    }

    if (!hasQuotes) {
        quotesText += "Пока нет цитат из этой книги";
    }

    QMessageBox::information(this, "Цитаты", quotesText);
}

// ============ ДОБАВИТЬ НА ПОЛКУ ============
void MainWindow::on_btnAddToShelf_clicked()
{
    if (currentBookId == -1) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите книгу!");
        return;
    }

    int shelfRow = ui->listShelves->currentRow();
    if (shelfRow == -1) {
        QMessageBox::warning(this, "Ошибка", "Выберите полку!");
        return;
    }

    QListWidgetItem *shelfItem = ui->listShelves->item(shelfRow);
    int shelfId = shelfItem->data(Qt::UserRole).toInt();

    addToShelf(currentBookId, shelfId);
}

// ============ ПОМЕСТИТЬ КНИГУ НА ПОЛКУ ============
void MainWindow::addToShelf(int bookId, int shelfId)
{
    // Проверяем, нет ли уже такой связи
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT 1 FROM book_shelves WHERE book_id = ? AND shelf_id = ?");
    checkQuery.addBindValue(bookId);
    checkQuery.addBindValue(shelfId);

    if (checkQuery.exec() && checkQuery.next()) {
        QMessageBox::information(this, "Информация",
            "Эта книга уже на этой полке!");
        return;
    }

    // Добавляем связь
    QSqlQuery query;
    query.prepare("INSERT INTO book_shelves (book_id, shelf_id) VALUES (?, ?)");
    query.addBindValue(bookId);
    query.addBindValue(shelfId);

    if (query.exec()) {
        // Обновляем список книг с новыми полками
        loadBooksFromDB();

        // Загружаем детали текущей книги заново
        if (currentBookId == bookId) {
            loadBookDetails(bookId);
        }

        QMessageBox::information(this, "Успех", "Книга добавлена на полку!");
    } else {
        QMessageBox::warning(this, "Ошибка",
            "Не удалось добавить на полку:\n" + query.lastError().text());
    }
}
// ============ РЕДАКТИРОВАНИЕ КНИГИ ============
void MainWindow::on_btnEdit_clicked()
{
    int row = ui->listBooks->currentRow();
    if (row == -1) {
        QMessageBox::warning(this, "Внимание", "Выберите книгу для редактирования!");
        return;
    }

    QListWidgetItem *item = ui->listBooks->item(row);
    int bookId = item->data(Qt::UserRole).toInt();

    QSqlQuery query;
    query.prepare("SELECT title, author, status, rating FROM books WHERE id = ?");
    query.addBindValue(bookId);

    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить данные книги");
        return;
    }

    QString currentTitle = query.value(0).toString();
    QString currentAuthor = query.value(1).toString();
    QString currentStatus = query.value(2).toString();
    int currentRating = query.value(3).toInt();

    bool ok;
    QString newTitle = QInputDialog::getText(this, "Редактировать",
        "Название:", QLineEdit::Normal, currentTitle, &ok);
    if (!ok) return;

    QString newAuthor = QInputDialog::getText(this, "Редактировать",
        "Автор:", QLineEdit::Normal, currentAuthor, &ok);
    if (!ok) return;

    QStringList statuses = {"planned", "reading", "finished"};
    QString currentStatusText;
    if (currentStatus == "planned") currentStatusText = "Хочу прочитать";
    else if (currentStatus == "reading") currentStatusText = "Читаю сейчас";
    else if (currentStatus == "finished") currentStatusText = "Прочитано";

    QString newStatusText = QInputDialog::getItem(this, "Редактировать",
        "Статус:", QStringList() << "Хочу прочитать" << "Читаю сейчас" << "Прочитано",
        statuses.indexOf(currentStatus), false, &ok);
    if (!ok) return;

    int newRating = QInputDialog::getInt(this, "Редактировать",
        "Оценка (0-5, 0 - не оценено):", currentRating, 0, 5, 1, &ok);
    if (!ok) return;

    QString newStatus;
    if (newStatusText == "Хочу прочитать") newStatus = "planned";
    else if (newStatusText == "Читаю сейчас") newStatus = "reading";
    else if (newStatusText == "Прочитано") newStatus = "finished";

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE books SET title = ?, author = ?, status = ?, rating = ? WHERE id = ?");
    updateQuery.addBindValue(newTitle);
    updateQuery.addBindValue(newAuthor);
    updateQuery.addBindValue(newStatus);
    updateQuery.addBindValue(newRating > 0 ? newRating : QVariant());
    updateQuery.addBindValue(bookId);

    if (updateQuery.exec()) {
        loadBooksFromDB();
        loadBookDetails(bookId);
        QMessageBox::information(this, "Успех", "Книга обновлена!");
    }
}
