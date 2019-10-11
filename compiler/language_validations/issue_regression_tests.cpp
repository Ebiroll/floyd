//
//  issue_regression_tests.cpp
//  Floyd
//
//  Created by Marcus Zetterquist on 2019-02-25.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "test_helpers.h"


using namespace floyd;


QUARK_TEST("https://github.com/marcusz/floyd/issues/8", "", "", "") {
	ut_verify_exception_nolib(
		QUARK_POS,
		
R"(for (i in 1..5) {
  print(i)
}
)",
		R"___([Syntax] For loop has illegal range syntax. Line: 1 "for (i in 1..5) {")___"
	);
}




