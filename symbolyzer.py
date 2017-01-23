import sys

def gen_key(name, index):
	try:
		return "".join(reversed(["%02x" % ord(name[index + i]) for i in range(4)]))
	except:
		print index, name
		raise

class functions:
	def __init__(self, names):
		self.names = names
		self.functions = []
		self.cant_match = []

	def gen_functions(self):
		for i in range(len(self.names)):
			name = self.names[i]
			j = 0
			for n in self.names:
				if n == name: continue
				k = self.find_unique_start(name, n)
				if k == -1:
					self.cant_match.append(name)
					break
				if k > j:
					j = k

			if name not in self.cant_match:
				self.functions.append(function(name, j))

	def find_unique_start(self, name1, name2):
		name1 += "\0"
		name2 += "\0"

		looplen = min(len(name1), len(name2)) - 3
		if looplen < 0: return -1

		i = 0
		for i in range(looplen):
			if gen_key(name1, i) != gen_key(name2, i):
				break

		if i == looplen:
			if len(name1) > len(name2):
				return i
			return -1

		return i

class function:
	def __init__(self, name, index):
		self.name = name
		self.set_index(index)

	def set_index(self, index):
		self.index = index
		self.key = gen_key(self.name + "\0", index)

	def __str__(self):
		return "%s[%i] = 0x%s" % (self.name, self.index, self.key)

if __name__ == "__main__":
	strings = []
	keys = []
	for func in sys.stdin.readlines():
		func = func.strip()
		if not func: continue 

		strings.append(func)

	f = functions(strings)
	f.gen_functions()

	for u in f.functions:
		print u

	if f.cant_match:
		print "\nDIDN'T WORK (%i):" % len(f.cant_match)
		for n in f.cant_match:
			print n
