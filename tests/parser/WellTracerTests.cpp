/*
  Copyright 2018 NORCE

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#define BOOST_TEST_MODULE WellTracerTests

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Well.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Parser/Parser.hpp>

using namespace Opm;

static Deck createDeckWithOutTracer() {
    Opm::Parser parser;
    std::string input =
            "GRID\n"
            "PERMX\n"
            "   1000*0.25/\n"
            "COPY\n"
            "  PERMX PERMY /\n"
            "  PERMX PERMZ /\n"
            "/\n"
            "SCHEDULE\n"
            "WELSPECS\n"
            "     'W_1'        'OP'   2   2  1*       \'OIL\'  7* /   \n"
            "/\n"
            "COMPDAT\n"
            " 'W_1'  2*  1   1 'OPEN' / \n"
            "/\n"
            "WCONINJE\n"
            "     'W_1' 'WATER' 'OPEN' 'BHP' 1 2 3/\n/\n";

    return parser.parseString(input);
}


static Deck createDeckWithDynamicWTRACER() {
    Opm::Parser parser;
    std::string input =
            "START             -- 0 \n"
            "1 JAN 2000 / \n"
            "GRID\n"
            "PERMX\n"
            "   1000*0.25/\n"
            "COPY\n"
            "  PERMX PERMY /\n"
            "  PERMX PERMZ /\n"
            "/\n"
            "SCHEDULE\n"
            "WELSPECS\n"
            "     'W_1'        'OP'   1   1  1*       \'GAS\'  7* /   \n"
            "/\n"
            "COMPDAT\n"
            " 'W_1'  2*  1   1 'OPEN' / \n"
            "/\n"
            "WCONINJE\n"
            "     'W_1' 'GAS' 'OPEN' 'BHP' 1 2 3/\n/\n"
            "DATES             -- 1\n"
            " 1  MAY 2000 / \n"
            "/\n"
            "WTRACER\n"
            "     'W_1' 'I1'       1 / \n "
            "     'W_1' 'I2'       1 / \n "
            "/\n"
            "DATES             -- 2, 3\n"
            " 1  JUL 2000 / \n"
            " 1  AUG 2000 / \n"
            "/\n"
            "WTRACER\n"
            "     'W_1' 'I1'       0 / \n "
            "/\n"
            "DATES             -- 4\n"
            " 1  SEP 2000 / \n"
            "/\n";

    return parser.parseString(input);
}

static Deck createDeckWithTracerInProducer() {
    Opm::Parser parser;
    std::string input =
            "START             -- 0 \n"
            "1 JAN 2000 / \n"
            "GRID\n"
            "PERMX\n"
            "   1000*0.25/\n"
            "COPY\n"
            "  PERMX PERMY /\n"
            "  PERMX PERMZ /\n"
            "/\n"
            "SCHEDULE\n"
            "WELSPECS\n"
            "     'W_1'        'OP'   1   1  1*       \'GAS\'  7* /   \n"
            "/\n"
            "COMPDAT\n"
            " 'W_1'  2*  1   1 'OPEN' / \n"
            "/\n"
            "WCONPROD\n"
                "'W_1' 'OPEN' 'ORAT' 20000  4* 1000 /\n"
            "WTRACER\n"
            "     'W_1' 'I1'       1 / \n "
            "     'W_1' 'I2'       1 / \n "
            "/\n";

    return parser.parseString(input);
}


BOOST_AUTO_TEST_CASE(TestNoTracer) {
    auto deck = createDeckWithOutTracer();
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec ( deck );
    Schedule schedule(deck, grid , eclipseProperties, runspec);
    BOOST_CHECK(!deck.hasKeyword("WTRACER"));
}


BOOST_AUTO_TEST_CASE(TestDynamicWTRACER) {
    auto deck = createDeckWithDynamicWTRACER();
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec ( deck );
    Schedule schedule(deck, grid , eclipseProperties, runspec);
    BOOST_CHECK(deck.hasKeyword("WTRACER"));
    const auto& keyword = deck.getKeyword("WTRACER");
    BOOST_CHECK_EQUAL(keyword.size(),1);
    const auto& record = keyword.getRecord(0);
    const std::string& wellNamesPattern = record.getItem("WELL").getTrimmedString(0);
    auto wells_Tracer = schedule.getWellsMatching(wellNamesPattern);
    BOOST_CHECK_EQUAL(wellNamesPattern, "W_1");
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(0).getConcentration("I1"),0); //default 0
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(0).getConcentration("I2"),0); //default 0
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(1).getConcentration("I1"),1);
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(2).getConcentration("I1"),1);
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(4).getConcentration("I1"),0);
    BOOST_CHECK_EQUAL(wells_Tracer[0]->getTracerProperties(4).getConcentration("I2"),1);
}


BOOST_AUTO_TEST_CASE(TestTracerInProducerTHROW) {
    auto deck = createDeckWithTracerInProducer();
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec ( deck );

    BOOST_CHECK_THROW(Schedule(deck, grid, eclipseProperties, runspec), std::invalid_argument);
}

