/*
   Copyright 2019 Equinor ASA.

   This file is part of the Open Porous Media project (OPM).

   OPM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   OPM is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with OPM.  If not, see <http://www.gnu.org/licenses/>.
   */

#ifndef ERFT_HPP
#define ERFT_HPP


#include "EclFile.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <ctime>
#include <map>
#include <set>


class ERft : public EclFile
{
public:
    explicit ERft(const std::string &filename);

    using RftDate = std::tuple<int,int,int>;
    template <typename T>
    const std::vector<T>& getRft(const std::string& name, const std::string& wellName,
                                 const RftDate& date) const;

    template <typename T>
    const std::vector<T>& getRft(const std::string& name, const std::string& wellName,
                                 int year, int month, int day) const;

    std::vector<std::string> listOfWells() const;
    std::vector<RftDate> listOfdates() const;

    using RftReportList = std::vector<std::pair<std::string, RftDate>>;
    const RftReportList& listOfRftReports() const { return rftReportList; }

    bool hasRft(const std::string& wellName, const RftDate& date) const;
    bool hasRft(const std::string& wellName, int year, int month, int day) const;

    std::vector<EclEntry> listOfRftArrays(const std::string& wellName,
                                          const RftDate& date) const;

    std::vector<EclEntry> listOfRftArrays(const std::string& wellName,
                                          int year, int month, int day) const;

    bool hasArray(const std::string& arrayName, const std::string& wellName,
                  const RftDate& date) const;

private:
    std::map<int, std::pair<int,int>> arrIndexRange;
    int numReports;
    std::vector<float> timeList;

    std::set<std::string> wellList;
    std::set<RftDate> dateList;
    RftReportList rftReportList;

    std::map<std::pair<std::string,RftDate>,int> reportIndex;  //  mapping report index to wellName and date (tupe)

    int getReportIndex(const std::string& wellName, const RftDate& date) const;
    int getArrayIndex(const std::string& name, const std::string& wellName,
                      const RftDate& date) const;
};

#endif

