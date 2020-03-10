/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libsolidity/analysis/ImmutableValidator.h>

#include <boost/range/adaptor/reversed.hpp>

using namespace solidity::frontend;

void ImmutableValidator::analyze()
{
	m_inConstructionContext = true;

	for (VariableDeclaration const* stateVar: m_currentContract.stateVariablesIncludingInherited())
		if (stateVar->value())
		{
			stateVar->value()->accept(*this);
			solAssert(m_initializedStateVariables.insert(stateVar).second, "");
		}

	for (auto const* contract: m_currentContract.annotation().linearizedBaseContracts  | boost::adaptors::reversed)
	{
		m_inConstructionContext = true;

		if (contract->constructor())
		{
			contract->constructor()->accept(*this);
			m_visitedCallables.insert(contract->constructor());
		}

		for (std::shared_ptr<InheritanceSpecifier> const inheritSpec: contract->baseContracts())
			if (auto args = inheritSpec->arguments())
				ASTNode::listAccept(*args, *this);

		m_inConstructionContext = false;

		for (auto funcDef: contract->definedFunctions())
			if (m_visitedCallables.find(funcDef) == m_visitedCallables.end())
				funcDef->accept(*this);

		for (auto modDef: contract->functionModifiers())
			if (m_visitedCallables.find(modDef) == m_visitedCallables.end())
				modDef->accept(*this);
	}

	checkAllVariablesInitialized(m_currentContract.location());
}

bool ImmutableValidator::visit(FunctionDefinition const& _functionDefinition)
{
	return analyseCallable(_functionDefinition);
}


bool ImmutableValidator::visit(ModifierDefinition const& _modifierDefinition)
{
	return analyseCallable(_modifierDefinition);
}


bool ImmutableValidator::visit(MemberAccess const& _memberAccess)
{
	_memberAccess.expression().accept(*this);

	if (auto funcType = dynamic_cast<FunctionType const*>(_memberAccess.annotation().type))
		if (
			(funcType->kind() == FunctionType::Kind::Internal ||
			funcType->kind() == FunctionType::Kind::Declaration) &&
			funcType->hasDeclaration()
		)
			if (m_visitedCallables.insert(dynamic_cast<CallableDeclaration const*>(&funcType->declaration())).second)
				funcType->declaration().accept(*this);

	return false;
}

bool ImmutableValidator::visit(IfStatement const& _ifStatement)
{
	bool prevInBranch = m_inBranch;

	_ifStatement.condition().accept(*this);

	m_inBranch = true;

	_ifStatement.trueStatement().accept(*this);

	if (auto falseStatement = _ifStatement.falseStatement())
		falseStatement->accept(*this);

	m_inBranch = prevInBranch;

	return false;
}


bool ImmutableValidator::visit(WhileStatement const& _whileStatement)
{
	bool prevInLoop = m_inLoop;
	m_inLoop = true;

	_whileStatement.condition().accept(*this);
	_whileStatement.body().accept(*this);

	m_inLoop = prevInLoop;

	return false;
}


bool ImmutableValidator::visit(Identifier const& _identifier)
{
	if (auto const callableDef = dynamic_cast<CallableDeclaration const*>(_identifier.annotation().referencedDeclaration))
	{
		auto finalDef = findFinalOverride(callableDef);

		if (m_visitedCallables.insert(finalDef).second)
			finalDef->accept(*this);

		return false;
	}

	auto const varDecl = dynamic_cast<VariableDeclaration const*>(_identifier.annotation().referencedDeclaration);

	if (!varDecl || !varDecl->isStateVariable() || !varDecl->immutable())
		return false;

	if (_identifier.annotation().lValueRequested && _identifier.annotation().ordinaryLAssignment)
	{
		if (!m_currentConstructor)
			m_errorReporter.typeError(
				_identifier.location(),
				"Immutable variables can only be initialized directly in the constructor.");
		else if (m_currentConstructor->annotation().contract->id() != varDecl->annotation().contract->id())
			m_errorReporter.typeError(
				_identifier.location(),
				"Immutable variables must be initialized in the constructor of the contract they are defined in.");
		else if (m_inLoop)
			m_errorReporter.typeError(
				_identifier.location(),
				"Immutable variables can only be initialized once, not in a while statement.");
		else if (m_inBranch)
			m_errorReporter.typeError(
				_identifier.location(),
				"Immutable variables must be initialized unconditionally, not in an if statement.");

		auto insertResult = m_initializedStateVariables.insert(varDecl);

		if (!insertResult.second)
			m_errorReporter.typeError(
				_identifier.location(),
				"Immutable state variable already initialized."
			);
	}
	else if (m_inConstructionContext)
		m_errorReporter.typeError(
			_identifier.location(),
			"Immutable variables cannot be read in the constructor or any function or modifier called by it.");

	return false;
}


bool ImmutableValidator::visit(Return const& _return)
{
	if (m_currentConstructor == nullptr)
		return true;

	if (auto retExpr = _return.expression())
		retExpr->accept(*this);

	checkAllVariablesInitialized(_return.location());

	return false;
}


bool ImmutableValidator::analyseCallable(CallableDeclaration const& _callableDeclaration)
{
	FunctionDefinition const* prevConstructor = m_currentConstructor;
	m_currentConstructor = nullptr;

	if (FunctionDefinition const* funcDef = dynamic_cast<decltype(funcDef)>(&_callableDeclaration))
	{
		if (funcDef->isConstructor())
			m_currentConstructor = funcDef;

		ASTNode::listAccept(funcDef->modifiers(), *this);

		if (funcDef->isImplemented())
			funcDef->body().accept(*this);
	}
	else if (ModifierDefinition const* modDef = dynamic_cast<decltype(modDef)>(&_callableDeclaration))
		modDef->body().accept(*this);

	m_currentConstructor = prevConstructor;

	return false;
}


void ImmutableValidator::checkAllVariablesInitialized(langutil::SourceLocation const& _location)
{
	for (VariableDeclaration const* varDecl: m_currentContract.stateVariablesIncludingInherited())
		if (varDecl->immutable())
			if (m_initializedStateVariables.find(varDecl) == m_initializedStateVariables.end())
				m_errorReporter.typeError(
					_location,
					langutil::SecondarySourceLocation().append("Not initialized: ", varDecl->location()),
					"Construction controlflow ends without initializing all immutable state variables."
				);
}


CallableDeclaration const* ImmutableValidator::findFinalOverride(CallableDeclaration const* _callable)
{
	if (!_callable->virtualSemantics())
		return _callable;

	if (auto originFuncDef = dynamic_cast<FunctionDefinition const*>(_callable))
		for (auto const* contract: m_currentContract.annotation().linearizedBaseContracts)
		{
				for (auto const* funcDef: contract->definedFunctions())
					if (funcDef->name() == originFuncDef->name())
					{
						FunctionTypePointer fpA = FunctionType(*funcDef).asCallableFunction(false);
						FunctionTypePointer fpB = FunctionType(*originFuncDef).asCallableFunction(false);
						if (fpA->hasEqualReturnTypes(*fpB) && fpA->hasEqualParameterTypes(*fpB))
							return funcDef;
					}
		}
	else if (dynamic_cast<ModifierDefinition const*>(_callable))
		for (auto const* contract: m_currentContract.annotation().linearizedBaseContracts)
			for (auto const* modDef: contract->functionModifiers())
				if (_callable->name() == modDef->name())
					return modDef;

	return _callable;
}
