import os
import shutil
import sys
import re
import subprocess

# TODO: delete this python script. It seems not used anywehere.

# If first define not provided assume empty string:
if(len(sys.argv)==3):
    sys.argv.append("\"\"")

# Exit with usage if incorrect number of args.
if(len(sys.argv)<4 or (len(sys.argv)%2)==1):
    print("Usage: python minifyHLSL.py inputHLSLFile outputHeader0 defines0 ... outputHeaderN definesN")
    print("  inputHLSLFile - Filename for input HLSL file")
    print("  outputHeaderN - Filename for Nth output C++ header file")
    print("  definesN - Defines to generate Nth output header, in format VARIABLE=VALUE (if defines0 omitted assumes empty string)")
    sys.exit(-1)

# Get input filename from args.
inputHLSLFile = sys.argv[1]

# Calculate output file count from number of args.
outputFileCount = int((len(sys.argv)-2)/2)

# Intro status.
print("minifyHLSL.py minifying %s to %d header files" % (inputHLSLFile, outputFileCount))

# Get list of output header filenames and defines from args.
ouputHeaderFiles = []
dxcDefines = []
for outputFileIdx in range(0,outputFileCount):
    ouputHeaderFiles.append(sys.argv[outputFileIdx*2+2])
    dxcDefines.append(sys.argv[outputFileIdx*2+3])

# String variable prefix used in generated headers.
# TODO: Could be command line arg.
stringPrefix = "g_"

# Iterate through output files
for outputFileIdx in range(0,outputFileCount):

    # Calculate temp preprocessed filename from output filename.
    preprocessedHLSLFile = ouputHeaderFiles[outputFileIdx] + ".preprocessed.hlsl"

    # Work out string variable name from prefix string and filename.
    baseOutputFile =os.path.splitext(os.path.basename(ouputHeaderFiles[outputFileIdx]))[0];
    stringName = stringPrefix+baseOutputFile;

    # Run DXC to generate single preprocessed HLSL file (with all includes and ifdefs expanded)
    dxcCommand = "dxc -D %s %s -P %s" % (dxcDefines[outputFileIdx], inputHLSLFile, preprocessedHLSLFile)
    print("Preprocessing %s with defines %s to %s" % (inputHLSLFile, dxcDefines[outputFileIdx], preprocessedHLSLFile))
    try:
        compileRes = subprocess.check_output(dxcCommand, shell=True, stderr=subprocess.STDOUT)
    except:
        # Exit if DXC command fails.
        print("Failed to run DXC command (is DXC in the path?):\n"+dxcCommand)
        sys.exit(-1)

    # Print DXC output with a warning, if we have some (should not have any.)
    if(len(compileRes)>0):
        print("WARNING: DXC produced unexpected output:"+dxcCommand)
        print(compileRes)

    # Open header output file (exit if open fails.)
    try:
        headerOutput = open(ouputHeaderFiles[outputFileIdx] , "w")
    except:
        print("Failed to open:"+ouputHeaderFiles[outputFileIdx])
        sys.exit(-1)

    # Open pre-processed HLSL file (exit if open fails.)
    print("Minifying to %s" % (ouputHeaderFiles[outputFileIdx]))
    try:
        preprocFile = open(preprocessedHLSLFile, "r")
    except:
        print("Failed to open:"+ouputHeaderFile)
        sys.exit(-1)

    # Begin C string in minified header.
    headerOutput.write('const std::string '+stringName+' = ')

    # Current header file output line.
    currOutputStr = ""

    # Iterate through all the lines.
    for ln in preprocFile.readlines():
        # Remove single line comments (TODO: Remove multi-line comments)
        ln = re.sub(r"//.*\n", "\n", ln)
        # Replace single backslash with double backslash in C source (8x required as python AND DXC needed escaping).
        ln = re.sub(r"\\", "\\\\\\\\", ln)
        # Escape newlines
        ln = re.sub(r"\n", "\\\\n", ln)
        # Remove extra non-newline whitespace.
        ln = re.sub(r"[^\S\r\n]+", " ", ln)
        # Escape quotes.
        ln = re.sub(r"\"", "\\\"", ln)

        # Append to current line in header file string.
        currOutputStr += ln

        # When current line longer than 100 chars, write to header string (wrapping in quotes and adding newline+backslash)
        if(len(currOutputStr)>100):
            headerOutput.write('\t\"'+currOutputStr+'\" \\\n')
            currOutputStr=""

    # Finish writing header file.
    headerOutput.write('\t\"'+currOutputStr+'\"')
    headerOutput.write(';\n')
    headerOutput.close()
