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
const auto Quote = "'";

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

string FixCardName(const char* text)
{
    string sanitized = Sanitized(text);
    
    string result;
    
    for (auto i = sanitized.c_str(); *i; ++i)
    {
        auto c = *i;
        
        if ('A' <= c && c <= 'Z')
            c += 32;
        
        if (('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || c == '&')
            result += c;
        else if (c == '(' && i[1] == 'V' && i[2] == ')')
            i += 2;
    }
    
    if (result.size() > 15 &&
        !memcmp(result.c_str() + result.size() - 15, "defensiveshield", 15))
    {
        result = result.substr(0, result.size() - 15);
    }
    
    if (result.size() > 3 &&
        !memcmp(result.c_str() + result.size() - 3, "ep1", 3))
    {
        result = result.substr(0, result.size() - 3);
    }
    
    return move(result);
}

string FixExpansion(const char* text)
{
    string result;
    
    for (auto i = text; *i; ++i)
    {
        if (IsAlphanumeric(*i)) result += *i;
    }
    
    return move(result);
}

int main(int argc, char** argv)
{
    ofstream sql("swccg.postgres.sql", ofstream::binary);
    ofstream code("swccg.cpp.txt", ofstream::binary);
    ofstream json("swccg.json", ofstream::binary);
    ofstream script("rename-cards.sh", ofstream::binary);
    sqlite3* db = nullptr;

    if (sql && code && json && script)
    {
        sql << "CREATE TABLE \"legacy_card_info\" (";
        json << "[";

        if (sqlite3_open("swccg_db.sqlite", &db) == SQLITE_OK)
        {
            cout << "opened database" << endl;

            sqlite3_stmt* statement = nullptr;

            if (sqlite3_prepare(
                db, "SELECT * FROM swd", -1, &statement, nullptr) == SQLITE_OK)
            {
                cout << "prepared statement" << endl;

                int columnCount = sqlite3_column_count(statement);
                for (int i = 0; i < columnCount; ++i)
                {
                    auto columnName = sqlite3_column_name(statement, i);

                    if (i > 0) sql << ',';

                    sql << "\n  \"" << columnName << "\" text";
                    code << "\nconst int CM_"
                        << columnName
                        << " = "
                        << i
                        << ";";
                }
                
                code << "\n\n";

                sql << "\n);\n\nINSERT INTO \"legacy_card_info\" (";

                for (int i = 0; i < columnCount; ++i)
                {
                    if (i > 0) sql << ", ";
                    
                    auto columnName = sqlite3_column_name(statement, i);

                    sql << '"' << columnName << '"';
                }

                sql << ") VALUES";

                int rowCount = 0;

                while (sqlite3_step(statement) == SQLITE_ROW)
                {
                    if (rowCount++ > 0)
                    {
                        sql << ",";
                        json << ",";
                    }

                    sql << "\n  (";
                    json << "\n  {";
                    
                    auto addScript = [&](
                        const char* expansion,
                        const char* group,
                        const char* name,
                        const char* id,
                        const char* idSuffix)
                    {
                        script
                            << "cp -n \"cards/starwars/"
                            << expansion
                            << "-"
                            << group
                            << "/large/"
                            << name
                            << ".gif\" cards/unified/"
                            << id
                            << idSuffix
                            << ".gif\n";
                    };
                    
                    auto cardType = (const char*)
                        sqlite3_column_text(statement, 3);
                    
                    auto expansion = FixExpansion(
                        (const char*)sqlite3_column_text(statement, 6));
                    
                    auto group = (const char*)sqlite3_column_text(statement, 2);
                    auto id = (const char*)sqlite3_column_text(statement, 0);
                        
                    if (strcmp(cardType, "Objective"))
                    {
                        auto cardName = FixCardName((const char*)
                            sqlite3_column_text(statement, 1));
                    
                        addScript(
                            expansion.c_str(),
                            group,
                            cardName.c_str(),
                            id,
                            "");
                    }
                    else
                    {
                        auto cardName = FixCardName((const char*)
                            sqlite3_column_text(statement, 17));
                    
                        addScript(
                            expansion.c_str(),
                            group,
                            cardName.c_str(),
                            id,
                            "a");
                        
                        cardName = FixCardName((const char*)
                            sqlite3_column_text(statement, 18));
                        
                        addScript(
                            expansion.c_str(),
                            group,
                            cardName.c_str(),
                            id,
                            "b");
                    }
                    
                    bool writeComma = false;

                    for (int i = 0; i < columnCount; ++i)
                    {
                        if (i > 0) sql << ", ";

                        auto text = (const char*)
                            sqlite3_column_text(statement, i);

                        if (HasText(text))
                        {
                            auto columnName = sqlite3_column_name(statement, i);
                            auto sanitized = Sanitized(text);
                            
                            sql << "'";
                            
                            if (writeComma) json << ",";
                            
                            json << "\n    \""
                                << columnName
                                << "\": \"";
                               
                            writeComma = true;
                                
                            for (auto c : sanitized)
                            {
                                if (c == '\'')
                                    sql << "''";
                                else
                                    sql << c;
                                
                                if (c == '"')
                                    json << "\\\"";
                                else if (c == '\n')
                                    json << "\\n";
                                else
                                    json << c;
                            }
                            
                            sql << "'";
                            json << "\"";
                            
                        }
                        else
                        {
                            sql << "NULL";
                        }
                    }

                    sql << ")";
                    json << "\n  }";
                }

                sql << ";\n";
                
                cout << "read " << rowCount << " rows" << endl;

                sqlite3_finalize(statement);
                statement = nullptr;
            }

            sqlite3_close(db);
            db = nullptr;
        }
        
        json << "]\n";

        script.close();
        json.close();
        code.close();
        sql.close();
    }

    return 0;
}

