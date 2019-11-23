#!/usr/bin/env python3

import argparse
import os
import json


root_dir = os.path.normpath(os.path.join(os.path.abspath(__file__), '../..'))
prelude_dir = os.path.normpath(os.path.join(root_dir, 'tests/runtime/prelude'))

unit_kinds = ['program', 'library']

def generate_project(dir, unit_name, sources, kind='Program', references=[prelude_dir], version='1.0'):
    project = read_project_file(dir, create_default=True)
    project['skipVersion'] = version
    unit_body = {
        'kind': kind,
        'sources': sources,
        'references': list([{'path': ref} for ref in references])
    }
    add_program_unit(project, unit_name, unit_body)
    write_project_file(dir, project)


def init_project(version='1.0', program_units=None, variables=None):
    return {
        'skipVersion': version,
        'variables': variables if variables else {},
        'programUnits': program_units if program_units else {}
    }


# Write project dictionary to specified directory in prettified format
# returns the project dictionary that was written
def write_project_file(dir, project=None):
    project_file = os.path.join(os.path.abspath(dir), 'skip.project.json')
    project_dict = project if project else init_project()
    try:
        with open(project_file, 'w+') as f:
            f.write( json.dumps(
                    project_dict,
                    indent=2,
                    separators=(',', ': ')))
    except IOError as e:
        exit('Error while opening directory "' + dir + '". Make sure you are properly specifying your project directory. Full error:\n' + str(e))
    return project_dict


# read in a project file as a dictionary
# create_default: create default project if the file is not found
def read_project_file(dir, create_default=False):
    try:
        return json.loads(
            open(os.path.join(os.path.abspath(dir), 'skip.project.json'), 'r')
            .read())
    except IOError as e:
        if create_default:
            return write_project_file(dir)
        else:
            exit('Error while opening skip.project.json: ' + str(e))


# Add a new programUnits entry to the project
# if erase_existing is true and there is already an entry for unit_name, that
# entry will be erased and rewritten
def add_program_unit(project, unit_name, unit_body, erase_existing=True):
    if 'programUnits' not in project:
        project['programUnits'] = {}
    if erase_existing or unit_name not in project['programUnits']:
        project['programUnits'][unit_name] = unit_body


def check_kind(kind):
    if kind.lower() not in unit_kinds:
        raise argparse.ArgumentTypeError('Kind must be one of the following: ' + ', '.join(unit_kinds))
    return kind.lower().capitalize()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--name','-n', help="program unit name (default first source basename)")
    parser.add_argument('--kind','-k', type=check_kind, default='Program', help="Type of unit (Program/Library)")
    parser.add_argument('--version','-v', default='1.0', help="Skip version used")
    parser.add_argument('--dir','-d', help="project directory (default first source dir)")
    parser.add_argument('--sources','-s', nargs='+', required=True, help="list of space separated source file paths")
    parser.add_argument('--references', '-r', nargs='+', default=[prelude_dir], help="list of space separated reference paths")
    args = parser.parse_args()

    dir = os.path.abspath(args.dir or os.path.dirname(args.sources[0]))
    # adjust the source/reference names to be relative to our source directory
    sources = list([os.path.relpath(src, dir) for src in args.sources])
    references = list([os.path.relpath(ref, dir) for ref in args.references])
    name = args.name or os.path.splitext(os.path.basename(args.sources[0]))[0]
    generate_project(dir, name, sources, args.kind, references, version=args.version)


if __name__ == '__main__':
    main()
