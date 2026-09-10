"""
Temporary file to view parsed toml file

"""
import toml


with open("template.toml","r") as TOMLFile:
    data = toml.loads(TOMLFile.read())
    print(data)

