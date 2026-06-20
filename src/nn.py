# Find available node numbers for a new room.
from pathlib import Path
import yaml

max_number = 0
room_path = Path("./rooms")

# Loop through files and read content
for file_path in room_path.rglob('*'):
	if file_path.is_file():  # Skip any folders
		fin = open(file_path,'r')
		room = yaml.safe_load(fin)
		fin.close()
		number = room['node']
		if number > max_number:
			max_number = number
print(str(max_number+1))
