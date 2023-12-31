local xlsx = require 'xlsx'

local workbook = xlsx.Workbook('test.xlsx')

print(workbook)

local testFile = io.stdout

for sheetIndex = 1, workbook:GetTotalWorksheets() do
    local sheet1 = workbook[sheetIndex]
    local maxRows = sheet1:GetTotalRows()
    local maxCols = sheet1:GetTotalCols()
    print(sheetIndex, "Dimension of " .. sheet1:GetSheetName() .. " (" .. maxRows .. ", " .. maxCols .. ")")
    print()
    for c = 0, maxCols - 1 do testFile:write(("%10d|"):format(c + 1)) end
    testFile:write('\n')
    
    local cell = sheet1:Cell(0, 0)
    print("type",cell:Type())
    for r = 0, maxRows - 1 do
        testFile:write(("%10d|"):format(r + 1))
        for c = 0, maxCols - 1 do
            local cell = sheet1:Cell(r, c)
            if cell then
                local cellType = cell:Type()
                if cellType == cell.UNDEFINED then
                    testFile:write('          |')

                elseif cellType == cell.BOOLEAN then
                    testFile:write(("%10s|"):format(cell:GetBoolean()  and  'TRUE'  or  'FALSE'))

                elseif cellType == cell.INT then
                    testFile:write(("%10d|"):format(cell:GetInteger()))

                elseif cellType == cell.DOUBLE then
                    testFile:write(("%10.6g|"):format(cell:GetDouble()))

                elseif cellType == cell.STRING then
                    testFile:write(("%10s|"):format(cell:GetString()))

                elseif cellType == cell.WSTRING then
                    testFile:write(("%10s|"):format(tostring(cell:GetWString())))
                end
            else
                testFile:write('          |')
            end
        end
        testFile:write('\n')
    end
    testFile:write('\n')
end