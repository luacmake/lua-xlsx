#include "olua.h"

OLUA_BEGIN_DECLS
#include "lxplib.h"
#include "luazip.h"
OLUA_END_DECLS

static const char *lxplom_src = R"~~~(
-- See Copyright Notice in license.html
-- $Id: lom.lua,v 1.6 2005/06/09 19:18:40 tuler Exp $

local lxp = require "lxp"

local tinsert, tremove = table.insert, table.remove
local assert, type, print = assert, type, print


local function starttag (p, tag, attr)
  local stack = p:getcallbacks().stack
  local newelement = {tag = tag, attr = attr}
  tinsert(stack, newelement)
end

local function endtag (p, tag)
  local stack = p:getcallbacks().stack
  local element = tremove(stack)
  assert(element.tag == tag)
  local level = #stack
  tinsert(stack[level], element)
end

local function text (p, txt)
  local stack = p:getcallbacks().stack
  local element = stack[#stack]
  local n = #element
  if type(element[n]) == "string" then
    element[n] = element[n] .. txt
  else
    tinsert(element, txt)
  end
end

local function parse (o)
  local c = { StartElement = starttag,
              EndElement = endtag,
              CharacterData = text,
              _nonstrict = true,
              stack = {{}}
            }
  local p = lxp.new(c)
  local status, err
  if type(o) == "string" then
    status, err = p:parse(o)
    if not status then return nil, err end
  else
    for l in pairs(o) do
      status, err = p:parse(l)
      if not status then return nil, err end
    end
  end
  status, err = p:parse()
  if not status then return nil, err end
  p:close()
  return c.stack[1][1]
end

return { parse = parse }
)~~~";

static const char *xmlize_src = R"~~~(
local _ENV = setmetatable({}, {__index = _ENV})

-- Loosely based on a PHP implementation.
local lom = require "lxp.lom"

local function xml_depth(vals)
	local valCount = #vals
    if valCount == 0 then
        return ""
    elseif valCount == 1  and type(vals[1]) ~= 'table' then
		return vals[1]
	end

	local orderTable = {}
	local children = { ['*'] = orderTable }

	for i = 1, #vals do
		local val = vals[i]
		if type(val) == "table" then
			children[#children + 1] = val.tag
			local tagEntry = children[val.tag]
			if not tagEntry then
				tagEntry = {}
				children[val.tag] = tagEntry
			end

			local entry = {}
			tagEntry[#tagEntry + 1] = entry
			orderTable[#orderTable + 1] = { val.tag, #tagEntry }

			entry['@'] = val.attr
			entry['#'] = xml_depth(val)
		else
			children[#children + 1] = val
		end
	end

	return children
end


function luaize(data)
	data = data:gsub('<%?xml.-%?>(.+)', "%1")
	data = '<root>' .. data .. '</root>'

	local vals, err = lom.parse(data)

    array = xml_depth(vals)

	return array
end


---------------------------------------------------------------------------------
-- Encoding routines stolen from Lua Element Tree.
local mapping = { ['&']  = "&amp;"  ,
                  ['<']  = "&lt;"   ,
                  ['>']  = "&gt;"   ,
                  ['"']  = "&quot;" ,
                  ["'"]  = "&apos;" , -- not used
                  ["\t"] = "&#9;"    ,
                  ["\r"] = "&#13;"   ,
                  ["\n"] = "&#10;"   }

local function map(symbols)
  local array = {}
  for _, symbol in ipairs(symbols) do
    table.insert(array, {symbol, mapping[symbol]})
  end
  return array
end

encoding = {}

encoding[1] = { map{'&', '<'}      ,
                map{'&', '<', '"'} }

encoding[2] = { map{'&', '<', '>'}      ,
                map{'&', '<', '>', '"'} }

encoding[3] = { map{'&', '\r', '<', '>'}                  ,
                map{'&', '\r', '\n', '\t', '<', '>', '"'} }

encoding[4] = { map{'&', '\r', '\n', '\t', '<', '>', '"'} ,
                map{'&', '\r', '\n', '\t', '<', '>', '"'} }

encoding["minimal"]   = encoding[1]
encoding["standard"]  = encoding[2]
encoding["strict"]    = encoding[3]
encoding["most"]      = encoding[4]

local _encode = function(text, encoding)
	for _, key_value in pairs(encoding) do
		text = text:gsub(key_value[1], key_value[2])
	end
	return text
end
---------------------------------------------------------------------------------

local srep = string.rep

local function xmlsave_recurse(indent, luaTable, xmlTable, maxIndentLevel)
	local tabs = ''
	if indent then
		if not maxIndentLevel  or indent <= maxIndentLevel then
			tabs = srep('\t', indent)
		end
	end
	local keys = {}
	local entryOrder
	if luaTable[1] then
		for _, key in ipairs(luaTable) do
			local whichIndex = keys[key]
			if not whichIndex then
				keys[key] = 0
				whichIndex = 0
			end
			whichIndex = whichIndex + 1
			keys[key] = whichIndex

			local section = luaTable[key]
			if not section then
				if not indent then
					-- Generally whitespace.
					xmlTable[#xmlTable + 1] = key
				end
			else
				local entry = section[whichIndex]
				if not entry then
					error('xmlsave: syntax bad')
				end

				xmlTable[#xmlTable + 1] = tabs .. '<' .. key

				local attributes = entry['@']
				if attributes then
					if not indent then
						for _, attrKey in ipairs(attributes) do
							xmlTable[#xmlTable + 1] = ' ' .. attrKey .. '="' .. attributes[attrKey] .. '"'
						end
					else
						for attrKey, attrValue in pairs(attributes) do
							if type(attrKey) == 'string' then
								xmlTable[#xmlTable + 1] = ' ' .. attrKey .. '="' .. attrValue .. '"'
							end
						end
					end
				end

				xmlTable[#xmlTable + 1] = '>'

				local elements = entry['#']
				if type(elements) == 'table' then
					if indent then
						xmlTable[#xmlTable + 1] = '\n'
					end
					xmlsave_recurse(indent and (indent + 1) or nil, elements, xmlTable, maxIndentLevel)
				else
					xmlTable[#xmlTable + 1] = _encode(elements, encoding[4][1])
				end

				if indent and type(elements) == 'table' then
					xmlTable[#xmlTable + 1] = tabs
				end
				xmlTable[#xmlTable + 1] = '</' .. key .. '>'
				if indent then
					xmlTable[#xmlTable + 1] = '\n'
				end
			end
		end
	else
		for key, value in pairs(luaTable) do
			if type(value) == 'table' then
				for _, entry in ipairs(value) do
					xmlTable[#xmlTable + 1] = tabs .. '<' .. key

					local attributes = entry['@']
					if attributes then
						if not indent then
							for _, attrKey in ipairs(attributes) do
								xmlTable[#xmlTable + 1] = ' ' .. attrKey .. '="' .. attributes[attrKey] .. '"'
							end
						else
							for attrKey, attrValue in pairs(attributes) do
								if type(attrKey) == 'string' then
									xmlTable[#xmlTable + 1] = ' ' .. attrKey .. '="' .. attrValue .. '"'
								end
							end
						end
					end

					xmlTable[#xmlTable + 1] = '>'

					local elements = entry['#']
					if type(elements) == 'table' then
						if indent then
							xmlTable[#xmlTable + 1] = '\n'
						end
						xmlsave_recurse(indent and (indent + 1) or nil, elements, xmlTable, maxIndentLevel)
					else
						xmlTable[#xmlTable + 1] = _encode(elements, encoding[4][1]) 
					end

					if indent and type(elements) == 'table' then
						xmlTable[#xmlTable + 1] = tabs
					end
					xmlTable[#xmlTable + 1] = '</' .. key .. '>'
					if indent then
						xmlTable[#xmlTable + 1] = '\n'
					end
				end
			end
		end
	end
end


function xmlize(outFilename, luaTable, indent, maxIndentLevel)
	local xmlTable = {}
	xmlsave_recurse(indent, luaTable, xmlTable, maxIndentLevel)
	local outText = table.concat(xmlTable)
	if outFilename == ':string' then
		return outText
	else
		local file = io.open(outFilename, "wt")
		file:write(table.concat(xmlTable))
		file:close()
	end
end

return _ENV
)~~~";

static const char *xlsx_src = R"~~~(
local ziparchive = require 'zip'
local xmlize = require 'xmlize'

local M = {}

local colRowPattern = "([a-zA-Z]*)(%d*)"

local function _xlsx_readdocument(xlsx, documentName)
    local file = xlsx.archive:open(documentName)
    if not file then return end
    local buffer = file:read('*a')
    file:close()
    return xmlize.luaize(buffer)
end

local __cellMetatable = {
    UNDEFINED = 0,
    INT = 1,
    DOUBLE = 2,
    STRING = 3,
    WSTRING = 4,
    FORMULA = 5,
    BOOLEAN = 6,

    Get = function(self)
        return self.value
    end,

    GetBoolean = function(self)
        return self.value
    end,

    GetInteger = function(self)
        return self.value
    end,

    GetDouble = function(self)
        return self.value
    end,

    GetString = function(self)
        return self.value
    end,
}

__cellMetatable.__index = __cellMetatable

function __cellMetatable:Type()
    return self.type
end

local function Cell(rowNum, colNum, value, type, formula)
    return setmetatable({
        row = tonumber(rowNum),
        column = colNum,
        value = value,
        type = type  or  __cellMetatable.UNDEFINED,
        formula = formula,
    }, __cellMetatable)
end

local __colTypeTranslator = {
    b = __cellMetatable.BOOLEAN,
    s = __cellMetatable.STRING,
}

local __sheetMetatable = {
    __load = function(self)
        local sheetDoc = _xlsx_readdocument(self.workbook, ("xl/worksheets/sheet%d.xml"):format(self.id))
        local sheetData = sheetDoc.worksheet[1]['#'].sheetData
        local rows = {}
        local columns = {}
        if sheetData[1]['#'].row then
            for _, rowNode in ipairs(sheetData[1]['#'].row) do
                local rowNum = tonumber(rowNode['@'].r)
                if not rows[rowNum] then
                    rows[rowNum] = {}
                end
                if rowNode['#'].c then
                    for _, columnNode in ipairs(rowNode['#'].c) do
                        -- Generate the proper column index.
                        local cellId = columnNode['@'].r
                        local colLetters = cellId:match(colRowPattern)
                        local colNum = 0
                        if colLetters then
                            local index = 1
                            repeat
                                colNum = colNum * 26
                                colNum = colNum + colLetters:byte(index) - ('A'):byte(1) + 1
                                index = index + 1
                            until index > #colLetters
                        end

                        local colType = columnNode['@'].t

                        local data
                        if columnNode['#'].v then
                            data = columnNode['#'].v[1]['#']
                            if colType == 's' then
                                colType = __cellMetatable.STRING
                                data = self.workbook.sharedStrings[tonumber(data) + 1]
                            elseif colType == 'str' then
                                colType = __cellMetatable.STRING
                            elseif colType == 'b' then
                                colType = __cellMetatable.BOOLEAN
                                data = data == '1'
                            else
                                local cellS = tonumber(columnNode['@'].s)
                                local cellStyle = cellS and self.workbook.styles.cellXfs[cellS - 1] or nil
                                if cellStyle then
                                    local numberStyle = cellStyle.numFmtId
                                    if not numberStyle then
                                        numberStyle = 0
                                    end
                                    if numberStyle == 0  or  numberStyle == 1 then
                                        colType = __cellMetatable.INT
                                    else
                                        colType = __cellMetatable.DOUBLE
                                    end
                                    data = tonumber(data)
                                else
                                    colType = __cellMetatable.STRING
                                    data = tostring(data)
                                end
                            end

                            --local formula
                            --if columnNode['#'].f then
                                --assert()
                            --end

                        else
                            colType = __colTypeTranslator[colType]
                        end

                        if not columns[colNum] then
                            columns[colNum] = {}
                        end
                        local cell = Cell(rowNum, colNum, data, colType, formula)
                        table.insert(rows[rowNum], cell)
                        table.insert(columns[colNum], cell)
                        self.__cells[cellId] = cell
                    end
                end
            end
        end
        self.__rows = rows
        self.__cols = columns
        self.loaded = true
    end,

    rows = function(self)
        if not self.loaded then
            self.__load()
        end
        return self.__rows
    end,

    cols = function(self)
        if not self.loaded then
            self.__load()
        end
        return self.__cols
    end,

    GetAnsiSheetName = function(self)
        return self.name
    end,

    GetUnicodeSheetName = function(self)
        return self.name
    end,

    GetSheetName = function(self)
        return self.name
    end,

    GetTotalRows = function(self)
        return #self.__rows
    end,

    GetTotalCols = function(self)
        return #self.__cols
    end,

    Cell = function(self, row, col)
        local key = ''
        local extraColIndex = math.floor(col / 26)
        if extraColIndex > 0 then
            key = string.char(string.byte('A') + (extraColIndex - 1))
        end
        key = key .. string.char(string.byte('A') + (col % 26))
        key = key .. (row + 1)
        return self.__cells[key]
    end,

    __tostring = function(self)
        return "xlsx.Sheet " .. self.name
    end
}

__sheetMetatable.__index = function(self, key)
    local value = __sheetMetatable[key]
    if value then return value end
    return self.__cells[key]
end


function Sheet(workbook, id, name)
    local self = {}
    self.workbook = workbook
    self.id = id
    self.name = name
    self.loaded = false
    self.__cells = {}
    self.__cols = {}
    self.__rows = {}
    setmetatable(self, __sheetMetatable)
    return self
end


local __workbookMetatableMembers = {
    GetTotalWorksheets = function(self)
        return #self.__sheets
    end,

    GetWorksheet = function(self, key)
        return self.__sheets[key]
    end,

    GetAnsiSheetName = function(self, key)
        return self:GetWorksheet(key).name
    end,

    GetUnicodeSheetName = function(self, key)
        return self:GetWorksheet(key).name
    end,

    GetSheetName = function(self, key)
        return self:GetWorksheet(key).name
    end,

    Sheets = function(self)
        local i = 0
        return function()
            i = i + 1
            return self.__sheets[i]
        end
    end
}


local __workbookMetatable = {
    __len = function(self)
        return #self.__sheets
    end,

    __index = function(self, key)
        local value = __workbookMetatableMembers[key]
        if value then return value end
        return self.__sheets[key]
    end
}


function M.Workbook(filename)
    local self = {}

    self.archive = ziparchive.open(filename)

    local sharedStringsXml = _xlsx_readdocument(self, 'xl/sharedStrings.xml')
    self.sharedStrings = {}

    if sharedStringsXml then
        for _, str in ipairs(sharedStringsXml.sst[1]['#'].si) do
            if str['#'].r then
                local concatenatedString = {}
                for _, rstr in ipairs(str['#'].r) do
                    local t = rstr['#'].t[1]['#']
                    if type(t) == 'string' then
                        concatenatedString[#concatenatedString + 1] = rstr['#'].t[1]['#']
                    end
                end
                concatenatedString = table.concat(concatenatedString)
                self.sharedStrings[#self.sharedStrings + 1] = concatenatedString
            else
                self.sharedStrings[#self.sharedStrings + 1] = str['#'].t[1]['#']
            end
        end
    end

    local stylesXml = _xlsx_readdocument(self, 'xl/styles.xml')
    self.styles = {}
    local cellXfs = {}
    self.styles.cellXfs = cellXfs
    if stylesXml then
        for _, xfXml in ipairs(stylesXml.styleSheet[1]['#'].cellXfs[1]['#'].xf) do
            local xf = {}
            local numFmtId = xfXml['@'].numFmtId
            if numFmtId then
                xf.numFmtId = tonumber(numFmtId)
            end
            cellXfs[#cellXfs + 1] = xf
        end
    end

    self.workbookDoc = _xlsx_readdocument(self, 'xl/workbook.xml')
    local sheets = self.workbookDoc.workbook[1]['#'].sheets
    self.__sheets = {}
    local id = 1
    for _, sheetNode in ipairs(sheets[1]['#'].sheet) do
        local name = sheetNode['@'].name
        local sheet = Sheet(self, id, name)
        sheet:__load()
        self.__sheets[id] = sheet
        self.__sheets[name] = sheet
        id = id + 1
    end
    setmetatable(self, __workbookMetatable)
    return self
end

return M

--[[
xlsx = M

local workbook = xlsx.Workbook('Book1.xlsx')
print("Book")
local sheet = workbook[1]
print(sheet:Cell(0, 52))
cell = sheet:Cell(0, 52)
print(cell.value)
print(cell:Get())
print(workbook:GetTotalWorksheets())
print(sheet:rows())
print(sheet:cols())
print(sheet.B1)


--]]

-- vim: set tabstop=4 expandtab:
)~~~";

static int luaopen_lxp_lom(lua_State *L) {
    luaL_loadstring(L, lxplom_src);
    lua_call(L, 0, 1);
    return 1;
}

static int luaopen_xmlize(lua_State *L) {
    luaL_loadstring(L, xmlize_src);
    lua_call(L, 0, 1);
    return 1;
}

OLUA_BEGIN_DECLS
OLUA_LIB int luaopen_xlsx(lua_State* L)
{
    olua_require(L, "lxp", luaopen_lxp);
    olua_require(L, "lxp.lom", luaopen_lxp_lom);
    olua_require(L, "xmlize", luaopen_xmlize);
    olua_require(L, "zip", luaopen_zip);

    luaL_loadstring(L, xlsx_src);
    lua_call(L, 0, 1);
    return 1;
}
OLUA_END_DECLS