
cppcheck $1 --force -iPCM-Service_Win -j $2 2> cppcheck.out

if [ -s cppcheck.out ]
then
        cat cppcheck.out
        exit 1
fi

echo No issues found

