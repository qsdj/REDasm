#include "listingitemmodel.h"
#include <redasm/disassembler/listing/listingdocument.h>
#include <redasm/support/demangler.h>
#include <redasm/plugins/loader.h>
#include "../themeprovider.h"
#include <QColor>

ListingItemModel::ListingItemModel(size_t itemtype, QObject *parent) : DisassemblerModel(parent), m_itemtype(itemtype) { }

void ListingItemModel::setDisassembler(const REDasm::DisassemblerPtr& disassembler)
{
    DisassemblerModel::setDisassembler(disassembler);
    auto& document = m_disassembler->document();

    this->beginResetModel();

    for(auto it = document->begin(); it != document->end(); it++)
    {
        if(!this->isItemAllowed(it->get()))
            continue;

        m_items.insert(it->get());
    }

    this->endResetModel();

    EVENT_CONNECT(document, changed, this, std::bind(&ListingItemModel::onListingChanged, this, std::placeholders::_1));
}

QModelIndex ListingItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if((row < 0) || (row >= m_items.size()))
        return QModelIndex();

    return this->createIndex(row, column, const_cast<REDasm::ListingItem*>(m_items[row]));
}

int ListingItemModel::rowCount(const QModelIndex &) const { return m_items.size(); }
int ListingItemModel::columnCount(const QModelIndex &) const { return 4; }

QVariant ListingItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Vertical)
        return QVariant();

    if(role == Qt::DisplayRole)
    {
        if(section == 0)
            return "Address";
        else if(section == 1)
            return "Symbol";
        else if(section == 2)
            return "R";
        else if(section == 3)
            return "Segment";
    }

    return DisassemblerModel::headerData(section, orientation, role);
}

QVariant ListingItemModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    REDasm::ListingItem* item = reinterpret_cast<REDasm::ListingItem*>(index.internalPointer());
    auto lock = REDasm::x_lock_safe_ptr(m_disassembler->document());
    const REDasm::Symbol* symbol = lock->symbol(item->address);

    if(!symbol)
        return QVariant();

    if(role == Qt::DisplayRole)
    {
        if(index.column() == 0)
            return S_TO_QS(REDasm::hex(symbol->address, m_disassembler->assembler()->bits()));

        if(index.column() == 1)
        {
            if(symbol->is(REDasm::SymbolTypes::WideStringMask))
                return S_TO_QS(REDasm::quoted(m_disassembler->readWString(symbol)));
            else if(symbol->is(REDasm::SymbolTypes::StringMask))
                return S_TO_QS(REDasm::quoted(m_disassembler->readString(symbol)));

            return S_TO_QS(REDasm::Demangler::demangled(symbol->name));
        }

        if(index.column() == 2)
            return QString::number(m_disassembler->getReferencesCount(symbol->address));

        if(index.column() == 3)
        {
            REDasm::Segment* segment = lock->segment(symbol->address);

            if(segment)
                return S_TO_QS(segment->name);

            return "???";
        }
    }
    else if(role == Qt::BackgroundRole)
    {
        if(symbol->isFunction() && symbol->isLocked())
            return THEME_VALUE("locked_bg");
    }
    else if(role == Qt::ForegroundRole)
    {
        if(index.column() == 0)
            return THEME_VALUE("address_list_fg");

        if(symbol->is(REDasm::SymbolTypes::String) && (index.column() == 1))
            return THEME_VALUE("string_fg");
    }

    return QVariant();
}

bool ListingItemModel::isItemAllowed(REDasm::ListingItem *item) const
{
    if(m_itemtype == REDasm::ListingItem::AllItems)
        return true;

    return m_itemtype == item->type;
}

void ListingItemModel::onListingChanged(const REDasm::ListingDocumentChanged *ldc)
{
    if(!this->isItemAllowed(ldc->item))
        return;

    if(ldc->isRemoved())
    {
        int idx = static_cast<int>(m_items.indexOf(ldc->item));
        this->beginRemoveRows(QModelIndex(), idx, idx);
        m_items.erase(static_cast<size_t>(idx));
        this->endRemoveRows();
    }
    else if(ldc->isInserted())
    {
        int idx = static_cast<int>(m_items.insertionIndex(ldc->item));
        this->beginInsertRows(QModelIndex(), idx, idx);
        m_items.insert(ldc->item);
        this->endInsertRows();
    }
}
