#include <iostream>       
#include <string>         
#include <stdexcept>      
#include <limits>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;

// Database Connection Class
class Database {
    sql::mysql::MySQL_Driver* driver;
    sql::Connection* con;

public:
    Database() {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", ""); // Update with your credentials
        con->setSchema("BankDB");
    }

    sql::Connection* getConnection() {
        return con;
    }

    ~Database() {
        delete con;
    }
};

// Bank Class with Additional Functions
class Bank {
    Database db;
    int nextAccountNumber = 1009;

public:
    // Create Account
    void createAccount(string name, double initialAmount) {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "INSERT INTO Accounts (accountNumber, name, balance) VALUES (?, ?, ?)");
            pstmt->setInt(1, nextAccountNumber++);
            pstmt->setString(2, name);
            pstmt->setDouble(3, initialAmount);
            pstmt->execute();
            delete pstmt;

            cout << "Account created successfully. Account Number: " << nextAccountNumber - 1 << endl;
        }
        catch (sql::SQLException& e) {
            cerr << "Error creating account: " << e.what() << endl;
        }
    }

    // Deposit Money
    void depositMoney(int accountNumber, double amount) {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "UPDATE Accounts SET balance = balance + ? WHERE accountNumber = ?");
            pstmt->setDouble(1, amount);
            pstmt->setInt(2, accountNumber);

            int affectedRows = pstmt->executeUpdate();
            if (affectedRows > 0) {
                // Log transaction
                logTransaction(accountNumber, "deposit", amount);
                cout << "Deposited: " << amount << " to Account Number: " << accountNumber << endl;
            }
            else {
                cout << "Account not found." << endl;
            }

            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "Error depositing money: " << e.what() << endl;
        }
    }

    // Withdraw Money
    void withdrawMoney(int accountNumber, double amount) {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT balance FROM Accounts WHERE accountNumber = ?");
            pstmt->setInt(1, accountNumber);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next() && res->getDouble("balance") >= amount) {
                delete pstmt;

                pstmt = con->prepareStatement("UPDATE Accounts SET balance = balance - ? WHERE accountNumber = ?");
                pstmt->setDouble(1, amount);
                pstmt->setInt(2, accountNumber);
                pstmt->executeUpdate();

                // Log transaction
                logTransaction(accountNumber, "withdrawal", amount);
                cout << "Withdrawn: " << amount << " from Account Number: " << accountNumber << endl;
            }
            else {
                cout << "Insufficient balance or account not found." << endl;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "Error withdrawing money: " << e.what() << endl;
        }
    }

    // Calculate Total Balance
    void calculateTotalBalance() {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement("SELECT SUM(balance) AS total FROM Accounts");
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next()) {
                cout << "Total Balance in All Accounts: " << res->getDouble("total") << endl;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "Error calculating total balance: " << e.what() << endl;
        }
    }

    // Calculate Expenditure
    void calculateExpenditure(int accountNumber) {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT SUM(amount) AS expenditure FROM Transactions WHERE accountNumber = ? AND type = 'withdrawal'");
            pstmt->setInt(1, accountNumber);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next()) {
                cout << "Total Expenditure for Account " << accountNumber << ": " << res->getDouble("expenditure") << endl;
            }
            else {
                cout << "No expenditure data found." << endl;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "Error calculating expenditure: " << e.what() << endl;
        }
    }

private:
    // Log Transaction
    void logTransaction(int accountNumber, string type, double amount) {
        try {
            sql::Connection* con = db.getConnection();
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "INSERT INTO Transactions (accountNumber, type, amount) VALUES (?, ?, ?)");
            pstmt->setInt(1, accountNumber);
            pstmt->setString(2, type);
            pstmt->setDouble(3, amount);
            pstmt->execute();
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "Error logging transaction: " << e.what() << endl;
        }
    }
};

// Main Function
int main() {
    Bank bank;
    string name;
    double initialAmount;
    int accountNumber;
    double amount;

    // Menu
    int choice;
    do {
        cout << "\n--- Banking System ---" << endl;
        cout << "1. Create Account" << endl;
        cout << "2. Deposit Money" << endl;
        cout << "3. Withdraw Money" << endl;
        cout << "4. Calculate Total Balance" << endl;
        cout << "5. Calculate Expenditure" << endl;
        cout << "6. Exit" << endl;
        cout << "Enter your choice: ";
        cin >> choice;

        switch (choice) {
        case 1:
            cout << "Enter name: ";
            cin.ignore();
            getline(cin, name);
            cout << "Enter initial deposit: ";
            cin >> initialAmount;
            bank.createAccount(name, initialAmount);
            break;
        case 2:
            cout << "Enter account number: ";
            cin >> accountNumber;
            cout << "Enter amount to deposit: ";
            cin >> amount;
            bank.depositMoney(accountNumber, amount);
            break;
        case 3:
            cout << "Enter account number: ";
            cin >> accountNumber;
            cout << "Enter amount to withdraw: ";
            cin >> amount;
            bank.withdrawMoney(accountNumber, amount);
            break;
        case 4:
            bank.calculateTotalBalance();
            break;
        case 5:
            cout << "Enter account number: ";
            cin >> accountNumber;
            bank.calculateExpenditure(accountNumber);
            break;
        case 6:
            cout << "Exiting..." << endl;
            break;
        default:
            cout << "Invalid choice! Please try again." << endl;
        }
    } while (choice != 6);

    return 0;
}
