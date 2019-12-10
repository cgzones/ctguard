#include "sqlitestatement.hpp"

#include <limits>
#include <sstream>

#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

sqlite_statement::sqlite_statement(const std::string & statement, sqlite_db & db) : m_statment{ nullptr, &::sqlite3_finalize }, m_db{ db }
{
    if (statement.size() >= static_cast<uintmax_t>(std::numeric_limits<int>::max())) {
        throw sqlite_exception{ "Sql statement too long (" + std::to_string(statement.size()) + ")" };
    }

    sqlite3_stmt * tmp;  // NOLINT(cppcoreguidelines-init-variables)

    if (sqlite3_prepare_v2(m_db.m_db.get(), statement.c_str(), static_cast<int>(statement.size()), &tmp, nullptr) != SQLITE_OK || tmp == nullptr) {
        throw sqlite_exception{ "Can not compile sql statement '" + statement + "': " + m_db.error_msg() };
    }

    m_statment = std::unique_ptr<sqlite3_stmt, decltype(&::sqlite3_finalize)>{ tmp, &::sqlite3_finalize };
}

void sqlite_statement::reset()
{
    if (sqlite3_reset(m_statment.get()) != SQLITE_OK) {
        throw sqlite_exception{ std::string("Can not reset sql statement: ") + m_db.error_msg() };
    }
}

void sqlite_statement::clear_bindings()
{
    if (sqlite3_clear_bindings(m_statment.get()) != SQLITE_OK) {
        throw sqlite_exception{ std::string("Can not clear sql statement bindings: ") + m_db.error_msg() };
    }
}

sqlite_statement::result sqlite_statement::run(bool query)
{
    const state_t state{ sqlite3_step(m_statment.get()) };
    if (state == SQLITE_DONE) {
        return sqlite_statement::result{ *this, state };
    }

    if (query && state == SQLITE_ROW) {
        return sqlite_statement::result{ *this, state };
    }

    throw sqlite_exception{ "Can not run sql statement (" + std::to_string(state) + "): " + m_db.error_msg() };
}

void sqlite_statement::bind(int i, const std::string & value)
{
    if (value.size() >= static_cast<uintmax_t>(std::numeric_limits<int>::max())) {
        throw sqlite_exception{ "Content string to bind too long (" + std::to_string(value.size()) + ")" };
    }

    if (const int ret = sqlite3_bind_text(m_statment.get(), i, value.c_str(), static_cast<int>(value.size()), SQLITE_STATIC); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind string value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, int value)
{
    if (const int ret = sqlite3_bind_int(m_statment.get(), i, value); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind int value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, long value)  // NOLINT(google-runtime-int)
{
    if (const int ret = sqlite3_bind_int64(m_statment.get(), i, value); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind long value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, sqlite3_int64 value)
{
    if (const int ret = sqlite3_bind_int64(m_statment.get(), i, value); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind sqlite3_int64 to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, unsigned value)
{
    if (value >= static_cast<uintmax_t>(std::numeric_limits<int>::max())) {
        throw sqlite_exception{ "Content unsigned int to bind too long (" + std::to_string(value) + ")" };
    }

    if (const int ret = sqlite3_bind_int(m_statment.get(), i, static_cast<int>(value)); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind unsigned value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, unsigned long value)  // NOLINT(google-runtime-int)
{
    if (value >= static_cast<uintmax_t>(std::numeric_limits<sqlite3_int64>::max())) {
        throw sqlite_exception{ "Content sqlite3_uint64 to bind too long (" + std::to_string(value) + ")" };
    }

    if (const int ret = sqlite3_bind_int64(m_statment.get(), i, static_cast<sqlite3_int64>(value)); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind unsigned long value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, sqlite3_uint64 value)
{
    if (value >= static_cast<uintmax_t>(std::numeric_limits<sqlite3_int64>::max())) {
        throw sqlite_exception{ "Content sqlite3_uint64 to bind too long (" + std::to_string(value) + ")" };
    }

    if (const int ret = sqlite3_bind_int64(m_statment.get(), i, static_cast<sqlite3_int64>(value)); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind sqlite3_uint64 value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

void sqlite_statement::bind(int i, const std::string_view & value)
{
    if (value.size() >= static_cast<uintmax_t>(std::numeric_limits<int>::max())) {
        throw sqlite_exception{ "Content string_view to bind too long (" + std::to_string(value.size()) + ")" };
    }

    if (const int ret = sqlite3_bind_text(m_statment.get(), i, value.begin(), static_cast<int>(value.size()), SQLITE_STATIC); ret != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind string_view value to statement (" + std::to_string(ret) + "): " + m_db.error_msg() };
    }
}

} /* namespace ctguard::libs::sqlite */
