#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
using namespace std;

const char OneHalf[] = { (char)0xc2, (char)0xbd, 0x00 };
const char OneQuarter[] = { (char)0xc2, (char)0xbc, 0x00 };
const char EAcute[] = { (char)0xc3, (char)0xa9, 0x00 };
const char DownTriangle[] = { (char)0xe2, (char)0x96, (char)0xbc, 0x00 };
const char UpTriangle[] = { (char)0xe2, (char)0x96, (char)0xb2, 0x00 };
const char Dot[] = { (char)0xe2, (char)0x80, (char)0xa2, 0x00 };
const char Diamond[] = { (char)0xe2, (char)0x99, (char)0xa2, 0x00 };
const auto Quote = "''";

inline bool IsAlphanumeric(char c)
{
    return ('a' <= c && c <= 'z')
        || ('A' <= c && c <= 'Z')
        || ('0' <= c && c <= '9');
}

inline bool HasText(const char* text)
{
    return text && *text;
}

string Sanitized(const char* text)
{
    string result;

    for (const char* i = text; *i; ++i)
    {
        int c = (unsigned char)*i;

        switch (c)
        {
            case '\\':
            {
                char tag[8] = "";
                char* j = tag;
                ++i;

                bool endsWithZero = false;

                while (IsAlphanumeric(*i))
                {
                    endsWithZero = *i == '0';
                    *j++ = *i++;
                }

                if (!strcmp(tag, "par"))
                    result += '\n';
                else if (!strcmp(tag, "/"))
                    (result += DownTriangle) += ' ';

                if (endsWithZero) --i;

                break;
            }

            case '(':
            {
                if (!strncmp(i, "(*)", 3))
                {
                    result += '(';
                    result += Dot;
                    result += ')';
                    i += 2;
                }
                else if (!strncmp(i, "(**)", 4))
                {
                    result += '(';
                    result += Dot;
                    result += Dot;
                    result += ')';
                    i += 3;
                }
                else if (!strncmp(i, "(***)", 5))
                {
                    result += '(';
                    result += Dot;
                    result += Dot;
                    result += Dot;
                    result += ')';
                    i += 4;
                }
                else if (!strncmp(i, "(*]", 3))
                {
                    result += Dot;
                    i += 2;
                }
                else
                {
                    result += '(';
                }

                break;
            }

            case '<':
                if (i[1] == '>')
                {
                    result += Diamond;
                    ++i;
                }
                else
                {
                    result += '<';
                }

                break;

            case ' ':
                while (i[1] == ' ') ++i;

                result += ' ';
                break;

            case '\'': result += Quote; break;

            case 13: break;
            case 133: result += "..."; break;
            case 146: result += Quote; break;
            case 148: result += '"'; break;
            case 180: result += Quote; break;
            case 188: result += OneQuarter; break;
            case 189: result += OneHalf; break;
            case 233: result += EAcute; break;
            default: result += *i; break;
        }
    }

    return move(result);
}

int main(int argc, char** argv)
{
    ofstream sql("swccg.postgres.sql", ofstream::binary);
    ofstream code("swccg.cpp.txt", ofstream::binary);
    sqlite3* db = nullptr;

    if (sql)
    {
        sql << "CREATE TABLE \"legacy_card_info\" (";

        if (sqlite3_open("swccg_db.sqlite", &db) == SQLITE_OK)
        {
            cout << "opened database" << endl;

            sqlite3_stmt* statement = nullptr;

            if (sqlite3_prepare(
                db, "SELECT * FROM swd", -1, &statement, nullptr) == SQLITE_OK)
            {
                cout << "prepared statement" << endl;
                
                code << "struct ColumnMapping\n{";

                int columnCount = sqlite3_column_count(statement);
                for (int i = 0; i < columnCount; ++i)
                {
                    auto columnName = sqlite3_column_name(statement, i);

                    if (i > 0) sql << ',';

                    sql << "\n  \"" << columnName << "\" text";
                    code << "\n    int " << columnName << " = -1;";
                }
                
                code << "\n};\n\n";

                sql << "\n);\n\nINSERT INTO \"legacy_card_info\" (";

                for (int i = 0; i < columnCount; ++i)
                {
                    if (i > 0) sql << ", ";
                    
                    auto columnName = sqlite3_column_name(statement, i);

                    sql << '"' << columnName << '"';
                    code << "else if (!strcmp(columnName, \""
                        << columnName
                        << "\"))\n    columnMapping."
                        << columnName
                        << " = i;\n";
                }

                sql << ") VALUES";

                int rowCount = 0;

                while (sqlite3_step(statement) == SQLITE_ROW)
                {
                    if (rowCount++ > 0) sql << ",";

                    sql << "\n  (";

                    for (int i = 0; i < columnCount; ++i)
                    {
                        if (i > 0) sql << ", ";

                        const char* text = (const char*)sqlite3_column_text(
                            statement, i);

                        if (HasText(text))
                        {
                            sql << "'" << Sanitized(text) << "'";
                        }
                        else
                        {
                            sql << "NULL";
                        }
                    }

                    sql << ")";
                }

                sql << ";\n";

                sqlite3_finalize(statement);
                statement = nullptr;
            }

            sqlite3_close(db);
            db = nullptr;
        }

        code.close();
        sql.close();
    }

    return 0;
}

