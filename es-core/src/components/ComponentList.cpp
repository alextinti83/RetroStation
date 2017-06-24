#include "components/ComponentList.h"
#include "Util.h"
#include "Log.h"

#define TOTAL_HORIZONTAL_PADDING_PX 20



ComponentList::ComponentList(Window* window, std::chrono::milliseconds scrollTierDelay)
	: IList<ComponentListRow, void*>(window, scrollTierDelay, LIST_NEVER_LOOP)
{
	mSelectorBarOffset = 0;
	mCameraOffset = 0;
	mFocused = false;
}

void ComponentList::addRow(const ComponentListRow& row, bool setCursorHere)
{
	IList<ComponentListRow, void*>::Entry e;
	e.name = "";
	e.object = NULL;
	e.data = row;

	this->add(e);

	for(auto it = mEntries.back().data.elements.begin(); it != mEntries.back().data.elements.end(); it++)
		addChild(it->component.get());

	updateElementSize(mEntries.back().data);
	updateElementPosition(mEntries.back().data);

	if(setCursorHere)
	{
		mCursor = mEntries.size() - 1;
		onCursorChanged(CURSOR_STOPPED);
	}
}


void ComponentList::onSizeChanged()
{
	for(auto it = mEntries.begin(); it != mEntries.end(); it++)
	{
		updateElementSize(it->data);
		updateElementPosition(it->data);
	}

	updateCameraOffset();
}

void ComponentList::onFocusLost()
{
	mFocused = false;
}

void ComponentList::onFocusGained()
{
	mFocused = true;
}

bool ComponentList::input(InputConfig* config, Input input)
{
	if(size() == 0)
		return false;

	// give it to the current row's input handler
	if(mEntries.at(mCursor).data.input_handler)
	{
		if(mEntries.at(mCursor).data.input_handler(config, input))
			return true;
	}else{
		// no input handler assigned, do the default, which is to give it to the rightmost element in the row
		auto& row = mEntries.at(mCursor).data;
		if(row.elements.size())
		{
			if(row.elements.back().component->input(config, input))
				return true;
		}
	}

	// input handler didn't consume the input - try to scroll
	if(config->isMappedTo("up", input))
	{
		return listInput(input.value != 0 ? -1 : 0);
	}else if(config->isMappedTo("down", input))
	{
		return listInput(input.value != 0 ? 1 : 0);

	}else if(config->isMappedTo("pageup", input))
	{
		return listInput(input.value != 0 ? -6 : 0);
	}else if(config->isMappedTo("pagedown", input)){
		return listInput(input.value != 0 ? 6 : 0);
	}

	return false;
}

void ComponentList::update(int deltaTime)
{
	listUpdate(deltaTime);

	if(size())
	{
		// update our currently selected row
		for(auto it = mEntries.at(mCursor).data.elements.begin(); it != mEntries.at(mCursor).data.elements.end(); it++)
			it->component->update(deltaTime);
	}
}

void ComponentList::onCursorChanged(const CursorState& state)
{
	// update the selector bar position
	// in the future this might be animated
	mSelectorBarOffset = 0;
	for(int i = 0; i < mCursor; i++)
	{
		mSelectorBarOffset += getRowHeight(mEntries.at(i).data);
	}

	updateCameraOffset();

	// this is terribly inefficient but we don't know what we came from so...
	if(size())
	{
		for(auto it = mEntries.begin(); it != mEntries.end(); it++)
			it->data.elements.back().component->onFocusLost();

		mEntries.at(mCursor).data.elements.back().component->onFocusGained();
	}

	if(mCursorChangedCallback)
		mCursorChangedCallback(state);

	updateHelpPrompts();
}

void ComponentList::updateCameraOffset()
{
	// move the camera to scroll
	const float totalHeight = getTotalRowHeight();
	if(totalHeight > mSize.y())
	{
		float target = mSelectorBarOffset + getRowHeight(mEntries.at(mCursor).data)/2 - (mSize.y() / 2);

		// clamp it
		mCameraOffset = 0;
		unsigned int i = 0;
		while(mCameraOffset < target && i < mEntries.size())
		{
			mCameraOffset += getRowHeight(mEntries.at(i).data);
			i++;
		}

		if(mCameraOffset < 0)
			mCameraOffset = 0;
		else if(mCameraOffset + mSize.y() > totalHeight)
			mCameraOffset = totalHeight - mSize.y();
	}else{
		mCameraOffset = 0;
	}
}

void ComponentList::render(const Eigen::Affine3f& parentTrans)
{
	if(!size())
		return;

	Eigen::Affine3f trans = roundMatrix(parentTrans * getTransform());

	// clip everything to be inside our bounds
	Eigen::Vector3f dim(mSize.x(), mSize.y(), 0);
	dim = trans * dim - trans.translation();
	Renderer::pushClipRect(Eigen::Vector2i((int)trans.translation().x(), (int)trans.translation().y()),
		Eigen::Vector2i((int)round(dim.x()), (int)round(dim.y() + 1)));

	// scroll the camera
	trans.translate(Eigen::Vector3f(0, -round(mCameraOffset), 0));

	// draw our entries
	std::vector<GuiComponent*> drawAfterCursor;
	bool drawAll;
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		auto& entry = mEntries.at(i);
		drawAll = !mFocused || i != mCursor;
		for(auto it = entry.data.elements.begin(); it != entry.data.elements.end(); it++)
		{
			if(drawAll || it->invert_when_selected)
			{
				it->component->render(trans);
			}else{
				drawAfterCursor.push_back(it->component.get());
			}
		}
	}

	// custom rendering
	Renderer::setMatrix(trans);

	// draw selector bar
	if(mFocused)
	{
		// inversion: src * (1 - dst) + dst * 0 = where src = 1
		// need a function that goes roughly 0x777777 -> 0xFFFFFF
		// and 0xFFFFFF -> 0x777777
		// (1 - dst) + 0x77

		const float selectedRowHeight = getRowHeight(mEntries.at(mCursor).data);
		Renderer::drawRect(0.0f, mSelectorBarOffset, mSize.x(), selectedRowHeight, 0xFFFFFFFF,
			GL_ONE_MINUS_DST_COLOR, GL_ZERO);
		Renderer::drawRect(0.0f, mSelectorBarOffset, mSize.x(), selectedRowHeight, 0x777777FF,
			GL_ONE, GL_ONE);

		// hack to draw 2px dark on left/right of the bar
		Renderer::drawRect(0.0f, mSelectorBarOffset, 2.0f, selectedRowHeight, 0x878787FF);
		Renderer::drawRect(mSize.x() - 2.0f, mSelectorBarOffset, 2.0f, selectedRowHeight, 0x878787FF);

		for(auto it = drawAfterCursor.begin(); it != drawAfterCursor.end(); it++)
			(*it)->render(trans);

		// reset matrix if one of these components changed it
		if(drawAfterCursor.size())
			Renderer::setMatrix(trans);
	}

	// draw separators
	float y = 0;
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, 0xC6C7C6FF);
		y += getRowHeight(mEntries.at(i).data);
	}
	Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, 0xC6C7C6FF);

	Renderer::popClipRect();
}

float ComponentList::getRowHeight(const ComponentListRow& row) const
{
	// returns the highest component height found in the row
	float height = 0;
	for(unsigned int i = 0; i < row.elements.size(); i++)
	{
		if(row.elements.at(i).component->getSize().y() > height)
			height = row.elements.at(i).component->getSize().y();
	}

	return height;
}

float ComponentList::getTotalRowHeight() const
{
	float height = 0;
	for(auto it = mEntries.begin(); it != mEntries.end(); it++)
	{
		height += getRowHeight(it->data);
	}

	return height;
}

void ComponentList::updateElementPosition(const ComponentListRow& row)
{
	float yOffset = 0;
	for(auto it = mEntries.begin(); it != mEntries.end() && &it->data != &row; it++)
	{
		yOffset += getRowHeight(it->data);
	}

	// assumes updateElementSize has already been called
	float rowHeight = getRowHeight(row);

	float x = TOTAL_HORIZONTAL_PADDING_PX / 2;
	for(unsigned int i = 0; i < row.elements.size(); i++)
	{
		const auto comp = row.elements.at(i).component;

		// center vertically
		comp->setPosition(x, (rowHeight - comp->getSize().y()) / 2 + yOffset);
		x += comp->getSize().x();
	}
}

void ComponentList::updateElementSize(const ComponentListRow& row)
{
	float width = mSize.x() - TOTAL_HORIZONTAL_PADDING_PX;
	std::vector< std::shared_ptr<GuiComponent> > resizeVec;

	for(auto it = row.elements.begin(); it != row.elements.end(); it++)
	{
		if(it->resize_width)
			resizeVec.push_back(it->component);
		else
			width -= it->component->getSize().x();
	}

	// redistribute the "unused" width equally among the components with resize_width set to true
	width = width / resizeVec.size();
	for(auto it = resizeVec.begin(); it != resizeVec.end(); it++)
	{
		(*it)->setSize(width, (*it)->getSize().y());
	}
}

void ComponentList::textInput(const char* text)
{
	if(!size())
		return;

	mEntries.at(mCursor).data.elements.back().component->textInput(text);
}

std::vector<HelpPrompt> ComponentList::getHelpPrompts()
{
	if(!size())
		return std::vector<HelpPrompt>();

	std::vector<HelpPrompt> prompts = mEntries.at(mCursor).data.elements.back().component->getHelpPrompts();

	if(size() > 1)
	{
		bool addMovePrompt = true;
		for(auto it = prompts.begin(); it != prompts.end(); it++)
		{
			if(it->first == "up/down" || it->first == "up/down/left/right")
			{
				addMovePrompt = false;
				break;
			}
		}

		if(addMovePrompt)
			prompts.push_back(HelpPrompt("up/down", "choose"));
	}

	return prompts;
}

bool ComponentList::moveCursor(int amt)
{
	bool ret = listInput(amt);
	listInput(0);
	return ret;
}

bool ComponentList::SetFocusPosition(FocusPosition position, bool focus)
{
	if (position == FocusPosition::Top)
	{
		mCursor = 0;
		
	} else if (position == FocusPosition::Bottom)
	{
		mCursor = size() - 1;		
	}
	if (focus)
	{
		onCursorChanged(CURSOR_STOPPED);
	}

	for (ComponentListElement& elem : mEntries.at(mCursor).data.elements)
	{
		if (elem.component->SetFocusPosition(position, focus)) return true;
	}

	mFocused = focus;
	return true;
}

