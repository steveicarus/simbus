
AC_INIT(simbus,0.0)

AC_PROG_CC([gcc cc])
AC_PROG_CXX([g++ c++])

AC_CHECK_PROG(IVERILOG,iverilog,iverilog,no)
if test $IVERILOG = no; then
AC_MSG_ERROR("Icarus Verilog compiler is required")
fi

AC_CHECK_PROG(IVERILOG_VPI, iverilog-vpi, iverilog-vpi, no)
if test $IVERILOG_VPI = no; then
AC_MSG_ERROR("Icarus Verilog VPI compiler is required")
fi

AC_CHECK_PROG(FLEX, flex, flex, no)
if test $FLEX = no; then
AC_MSG_ERROR("flex is required")
fi

AC_CHECK_PROG(BISON, bison, bison, no)
if test $BISON = no; then
AC_MSG_ERROR("bison is required")
fi

AC_PROG_INSTALL

AC_CONFIG_FILES(Make.rules)
AC_OUTPUT
