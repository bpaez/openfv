import re
import sys
from tempfile import mkstemp
from shutil import move
from os import remove, close

# returns a list of strings
# enters under the "DocString"
def parse_doc_string(istr):
    param_pattern = re.compile(r'\\(\w+)\s+(.*)')
    line_comment_pattern = re.compile(r'//!\s*(\w+)\s+(.*)')
    block_comment_pattern = re.compile(r'/\*!\s*(\w+)\s+(.*)')
    docstring = list()
    # for line in map(lambda s : s.strip(), istr):
    for line in istr:
        line = line.strip()
        if line_comment_pattern.match(line):
            doc = line_comment_pattern.match(line).group()
            doc = doc.lstrip('//! ')
            docstring.append(doc)
            return docstring
        if block_comment_pattern.match(line):
            doc = block_comment_pattern.match(line).group()
            doc = doc.lstrip('/*! ')
            docstring.append(doc)
        if param_pattern.match(line):
            match = param_pattern.match(line)
            docstring.append(match.group(1) + ": " + match.group(2))
        if line == '*/':
            return docstring

def extract(istr, docstrings):
    pattern = re.compile(r'//\s*DocString:\s*(\w+)')
    # for line in map(lambda s : s.strip(), istr):
    for line in istr:
        line = line.strip()
        match = pattern.match(line)
        if match:
            # 'token' here refers to the function name
            token = match.group(1)
            docstrings[token] = parse_doc_string(istr)

def format_doc_string(docstring):
    return '\n'.join(line for line in docstring)

def escape(string):
    return string.replace('\n', r'\n')

def substitute(istr, docfile, docstrings):
    pattern = re.compile(r'@DocString\((\w+)\)')
    fh, abs_path = mkstemp()
    with open(abs_path,'w') as new_file:
        for line in map(lambda s : s.rstrip(), istr):
            for match in pattern.finditer(line):
                token = match.group(1)
                docstring = format_doc_string(docstrings[token])
                line = line.replace(match.group(0), escape(docstring))
            new_file.write(line + "\n")
        # print(line, file=ostr)
    close(fh)
    remove(docfile)
    move(abs_path, docfile)

if __name__ == '__main__':
    sourcefile = sys.argv[1]
    docfile = sys.argv[2]
    docstrings = dict()
    with open(sourcefile) as istr:
        extract(istr, docstrings)
    with open(docfile) as istr:
        # with sys.stdout as ostr:
        substitute(istr, docfile, docstrings)
