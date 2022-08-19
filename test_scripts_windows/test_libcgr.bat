@Echo off

START "Unit tests" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\tests\unit_tests\unit-tests.exe" "--run_test=DijkstraPyCGRTutorialTestCase,DijkstraRoutingTestCase,DijkstraRoutingNoPathTestCase"