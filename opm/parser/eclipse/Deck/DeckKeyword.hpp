/*
 * File:   DeckKeyword.hpp
 * Author: kflik
 *
 * Created on June 3, 2013, 12:55 PM
 */

#ifndef DECKKEYWORD_HPP
#define DECKKEYWORD_HPP

#include <string>
#include <vector>
#include <memory>

#include <opm/parser/eclipse/Deck/DeckRecord.hpp>

namespace Opm {
    class ParserKeyword;

    class DeckKeyword {
    public:
        typedef std::vector< DeckRecord >::const_iterator const_iterator;

        DeckKeyword(const std::string& keywordName);
        DeckKeyword(const std::string& keywordName, bool knownKeyword);

        const std::string& name() const;
        void setLocation(const std::string& fileName, int lineNumber);
        const std::string& getFileName() const;
        int getLineNumber() const;

        size_t size() const;
        void addRecord(DeckRecord&& record);
        const DeckRecord& getRecord(size_t index) const;
        DeckRecord& getRecord(size_t index);
        const DeckRecord& getDataRecord() const;
        void setDataKeyword(bool isDataKeyword = true);
        bool isKnown() const;
        bool isDataKeyword() const;

        const std::vector<int>& getIntData() const;
        const std::vector<double>& getRawDoubleData() const;
        const std::vector<double>& getSIDoubleData() const;
        const std::vector<std::string>& getStringData() const;
        size_t getDataSize() const;

        template <class Keyword>
        bool isKeyword() const {
            if (Keyword::keywordName == m_keywordName)
                return true;
            else
                return false;
        }

        const_iterator begin() const;
        const_iterator end() const;

    private:
        std::string m_keywordName;
        std::string m_fileName;
        int m_lineNumber;

        std::vector< DeckRecord > m_recordList;
        bool m_knownKeyword;
        bool m_isDataKeyword;
    };
}

#endif  /* DECKKEYWORD_HPP */

