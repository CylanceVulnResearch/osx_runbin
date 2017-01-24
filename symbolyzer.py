######################################################################################
#                                                                                    #
#  Author: Stephanie Archibald <sarchibald@cylance.com>                              #
#  Copyright (c) 2017 Cylance Inc. All rights reserved.                              #
#                                                                                    #
#  Redistribution and use in source and binary forms, with or without modification,  #
#  are permitted provided that the following conditions are met:                     #
#                                                                                    #
#  1. Redistributions of source code must retain the above copyright notice, this    #
#  list of conditions and the following disclaimer.                                  #
#                                                                                    #
#  2. Redistributions in binary form must reproduce the above copyright notice,      #
#  this list of conditions and the following disclaimer in the documentation and/or  #
#  other materials provided with the distribution.                                   #
#                                                                                    #
#  3. Neither the name of the copyright holder nor the names of its contributors     #
#  may be used to endorse or promote products derived from this software without     #
#  specific prior written permission.                                                #
#                                                                                    #
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND   #
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED     #
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE            #
#  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR  #
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES    #
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;      #
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON    #
#  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT           #
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS     #
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                      #
#                                                                                    #
######################################################################################

import sys

def calc_int(string, offset):
	"""
	Return a hex string representation of the integer found by referencing offset bytes into string.
	"""
	try:
		return "".join(reversed(["%02x" % ord(string[offset + i]) for i in range(4)]))
	except:
		print offset, string
		raise

class results:
	"""
	Class to generate the offset / int pairs for a list of symbols.
	"""
	def __init__(self, symbols):
		self.symbols = symbols
		self.results = []
		self.cant_match = []
		self.generate()

	def generate(self):
		"""
		Create the offset / int pairs for all the symbols.
		"""
		for i in range(len(self.symbols)):
			symbol = self.symbols[i]
			j = 0
			for n in self.symbols:
				if n == symbol: continue
				k = self.find_unique_offset(symbol, n)
				if k == -1:
					self.cant_match.append(symbol)
					break
				if k > j:
					j = k

			if symbol not in self.cant_match:
				self.results.append(result(symbol, j))

	def find_unique_offset(self, symbol1, symbol2):
		"""
		Finds a unique offset between two symbols.
		"""
		symbol1 += "\0"
		symbol2 += "\0"

		looplen = min(len(symbol1), len(symbol2)) - 3
		if looplen < 0: return -1

		i = 0
		for i in range(looplen):
			if calc_int(symbol1, i) != calc_int(symbol2, i):
				break

		if i == looplen:
			if len(symbol1) > len(symbol2):
				return i
			return -1

		return i

class result:
	"""
	Helper class to store and print the offset / int pairs for a given symbol.
	"""
	def __init__(self, name, offset):
		self.name = name
		self.set_offset(offset)

	def set_offset(self, offset):
		self.offset = offset
		self.int = calc_int(self.name + "\0", offset)

	def __str__(self):
		return "%s[%i] = 0x%s" % (self.name, self.offset, self.int)

if __name__ == "__main__":
	try:
		r = results([line.strip() for line in sys.stdin.readlines() if line != "\n"])

		for p in r.results:
			print p

		if r.cant_match:
			print "\nDIDN'T WORK (%i):" % len(r.cant_match)
			for n in r.cant_match:
				print n
	except KeyboardInterrupt:
		pass
