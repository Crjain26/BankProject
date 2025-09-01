#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <cstdlib>  // For getenv
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

using namespace std;

std::mutex dbMutex; // for thread-safe operations

// Database Connection Class
class Database {
    sql::mysql::MySQL_Driver* driver;
    sql::Connection* con;

public:
    Database() {
        string host = "tcp://127.0.0.1:3306";
        string user = getenv("DB_USER") ? getenv("DB_USER") : "root";
        string pass = getenv("DB_PASS") ? getenv("DB_PASS") : "";

        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(host, user, pass);

        // Set SSL options (if configured in MySQL)
        // con->setClientOption("CLIENT_SSL", "true");

        con->setSchema("BankDB");
        con->setAutoCommit(false); // enable transactions
    }

    sql::Connection* getConnection() {
        return con;
    }

    ~Database() {
        delete con;
    }
};

// Bank Class
class Bank {
    Database db;

public:
    // User Login
    bool login(string username, string password) {
        sql::Connection* con = db.getConnection();
        sql::PreparedStatement* pstmt = con->prepareStatement(
            "SELECT * FROM Users WHERE username = ? AND password = SHA2(?, 256)");
        pstmt->setString(1, username);
        pstmt->setString(2, password);
        sql::ResultSet* res = pstmt->executeQuery();

        bool success = res->next();
        delete res;
        delete pstmt;

        return success;
    }

    // Create Account
    void createAccount(string name, double initialAmount) {
        lock_guard<mutex> lock(dbMutex);
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "INSERT INTO Accounts (name, balance) VALUES (?, ?)");
            pstmt->setString(1, name);
            pstmt->setDouble(2, initialAmount);
            pstmt->execute();

            con->commit();
            delete pstmt;

            cout << "Account created successfully for " << name << endl;
        } catch (sql::SQLException& e) {
            db.getConnection()->rollback();
            cerr << "Error creating account: " << e.what() << endl;
        }
    }

    // Deposit Money
    void depositMoney(int accountNumber, double amount) {
        lock_guard<mutex> lock(dbMutex);
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "UPDATE Accounts SET balance = balance + ? WHERE accountNumber = ?");
            pstmt->setDouble(1, amount);
            pstmt->setInt(2, accountNumber);

            int rows = pstmt->executeUpdate();
            if (rows > 0) {
                logTransaction(accountNumber, "deposit", amount);
                con->commit();
                cout << "Deposited: " << amount << " to Account " << accountNumber << endl;
            } else {
                cout << "Account not found." << endl;
                con->rollback();
            }
            delete pstmt;
        } catch (sql::SQLException& e) {
            db.getConnection()->rollback();
            cerr << "Error depositing: " << e.what() << endl;
        }
    }

    // Withdraw Money
    void withdrawMoney(int accountNumber, double amount) {
        lock_guard<mutex> lock(dbMutex);
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT balance FROM Accounts WHERE accountNumber = ?");
            pstmt->setInt(1, accountNumber);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next() && res->getDouble("balance") >= amount) {
                delete pstmt;

                pstmt = con->prepareStatement(
                    "UPDATE Accounts SET balance = balance - ? WHERE accountNumber = ?");
                pstmt->setDouble(1, amount);
                pstmt->setInt(2, accountNumber);
                pstmt->executeUpdate();

                logTransaction(accountNumber, "withdrawal", amount);
                con->commit();
                cout << "Withdrawn: " << amount << " from Account " << accountNumber << endl;
            } else {
                cout << "Insufficient balance or account not found." << endl;
                con->rollback();
            }

            delete res;
            delete pstmt;
        } catch (sql::SQLException& e) {
            db.getConnection()->rollback();
            cerr << "Error withdrawing: " << e.what() << endl;
        }
    }

private:
    // Log Transaction
    void logTransaction(int accountNumber, string type, double amount) {
        sql::Connection* con = db.getConnection();
        sql::PreparedStatement* pstmt = con->prepareStatement(
            "INSERT INTO Transactions (accountNumber, type, amount) VALUES (?, ?, ?)");
        pstmt->setInt(1, accountNumber);
        pstmt->setString(2, type);
        pstmt->setDouble(3, amount);
        pstmt->execute();
        delete pstmt;
    }
};

// Simulate concurrent users
void simulateUser(Bank& bank, int account, double amount, bool deposit) {
    if (deposit)
        bank.depositMoney(account, amount);
    else
        bank.withdrawMoney(account, amount);
}

int main() {
    Bank bank;

    string username, password;
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;

    if (!bank.login(username, password)) {
        cout << "Authentication failed!" << endl;
        return 0;
    }

    cout << "Login successful!" << endl;

    // Example concurrent deposits/withdrawals
    thread t1(simulateUser, ref(bank), 1001, 500.0, true);
    thread t2(simulateUser, ref(bank), 1001, 200.0, false);

    t1.join();
    t2.join();

    return 0;
}
